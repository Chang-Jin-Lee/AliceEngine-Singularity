// SPDX-License-Identifier: MIT
// The same parsed document drives the inspector, preview and validated save.
#include "AliceEditor/DocumentModel.h"
#include "Foundation/FileSystem.h"
#include "Schema/Registry.h"
#include "Schema/Validator.h"
#include "Verbs/SemanticCheck.h"
#include <cmath>
#include <limits>

namespace alice::editor {
namespace {
Diagnostic Error(std::string code, std::string message, const std::string& path, std::string hint) {
    auto d = MakeError(std::move(code), std::move(message), std::move(hint));
    d.file = path; d.path = "document"; d.mark = {1, 1, 0}; return d;
}
void Check(doc::Document& document, DiagnosticBag& bag) {
    const auto& schemas = schema::Registry::Global();
    const auto found = schemas.Find(document.SchemaId());
    if (!found) {
        bag.Add(Error("editor.schema.unknown", "Document schema is missing or unknown", document.path,
            "Set schema to an ID listed by alice schema list"));
    } else {
        schema::ValidateOptions options; options.maxDiagnostics = std::numeric_limits<usize>::max();
        schema::Validate(document.root, *found, schemas, bag, options);
        if (!bag.HasErrors()) verbs::CheckDocumentSemantics(document.root, verbs::VerbRegistry::Global(), schemas, bag, document.path);
    }
    bag.SetFileIfEmpty(document.path);
    for (auto& d : bag.Items()) {
        if (!d.mark.Valid()) d.mark = {1, 1, 0};
        if (d.hint.empty()) d.hint = "Correct the indicated field using its schema and validate again";
    }
}
Status FirstError(const DiagnosticBag& bag) {
    for (const auto& d : bag.Items()) if (d.severity == Severity::Error) return d;
    return Status::Ok();
}
}
Status DocumentModel::Open(const std::string& path) {
    const auto read = fs::ReadTextFile(path);
    if (!read) return read.Error();
    m_path = path; m_text = read.Value(); m_saved = m_text;
    m_undo.clear(); m_redo.clear(); Validate(); return Status::Ok();
}
Status DocumentModel::CreateScene(const std::string& path, const std::string& name) {
    if (name.empty()) return Error("editor.scene.name_empty", "Scene name cannot be empty", path, "Enter a nonempty scene name");
    doc::Document document; document.path = path; document.syntax = doc::DetectSyntax(path, {});
    document.root = doc::Value::MakeMap();
    document.root.Set("schema", doc::Value{"alice/scene/1"});
    document.root.Set("name", doc::Value{name});
    document.root.Set("actors", doc::Value::MakeSeq());
    DiagnosticBag bag; Check(document, bag); ALICE_TRY(FirstError(bag));
    ALICE_TRY(AtomicCreate(path, doc::SerializeDocument(document)));
    return Open(path);
}
bool DocumentModel::Validate() {
    m_diagnostics.Clear(); m_document = {};
    m_parsed = doc::ParseDocument(m_text, doc::Syntax::Auto, m_path, m_document, m_diagnostics);
    if (m_parsed) Check(m_document, m_diagnostics);
    m_diagnostics.SetFileIfEmpty(m_path);
    for (auto& diagnostic : m_diagnostics.Items()) {
        if (!diagnostic.mark.Valid()) diagnostic.mark = {1, 1, 0};
        if (diagnostic.path.empty()) diagnostic.path = "document";
        if (diagnostic.hint.empty()) diagnostic.hint = "Correct the syntax at the indicated line and validate the document again";
    }
    return IsValid();
}
void DocumentModel::Remember() {
    if (m_undo.size() == 128) m_undo.erase(m_undo.begin());
    m_undo.push_back(m_text); m_redo.clear();
}
void DocumentModel::SetText(std::string text) {
    if (text == m_text) return;
    Remember(); m_text = std::move(text); Validate();
}
bool DocumentModel::Undo() {
    if (m_undo.empty()) return false;
    m_redo.push_back(m_text); m_text = std::move(m_undo.back()); m_undo.pop_back(); Validate(); return true;
}
bool DocumentModel::Redo() {
    if (m_redo.empty()) return false;
    m_undo.push_back(m_text); m_text = std::move(m_redo.back()); m_redo.pop_back(); Validate(); return true;
}
Status DocumentModel::Save() {
    if (!Validate()) return FirstError(m_diagnostics);
    ALICE_TRY(AtomicSave(m_path, m_text, m_saved));
    m_saved = m_text; return Status::Ok();
}
std::vector<Actor> DocumentModel::Actors() const {
    std::vector<Actor> actors;
    if (!IsScene()) return actors;
    for (const auto& value : m_document.root["actors"].Items()) {
        Actor actor; actor.name = value["name"].AsString();
        for (usize i = 0; i < 3; ++i) {
            actor.position[i] = value["transform"]["position"].At(i).AsFloat(0);
            actor.rotation[i] = value["transform"]["rotation"].At(i).AsFloat(0);
            actor.scale[i] = value["transform"]["scale"].At(i).AsFloat(1);
        }
        actors.push_back(std::move(actor));
    }
    return actors;
}
Status DocumentModel::Commit(doc::Document document) {
    DiagnosticBag bag; Check(document, bag);
    ALICE_TRY(FirstError(bag));
    SetText(doc::SerializeDocument(document)); return Status::Ok();
}
Status DocumentModel::EditActor(usize index, const Actor& actor) {
    if (!IsValid() || !IsScene() || index >= Actors().size()) return Error("editor.actor.invalid",
        "Select an actor in a valid scene", m_path, "Fix document errors and select an existing actor");
    if (actor.name.empty()) return Error("editor.actor.name_empty", "Actor name cannot be empty", m_path, "Enter a nonempty name");
    for (const auto* vector : {&actor.position, &actor.rotation, &actor.scale})
        for (const auto number : *vector) if (!std::isfinite(number)) return Error("editor.actor.number_invalid",
            "Transform values must be finite", m_path, "Enter finite numbers for each transform axis");
    auto document = m_document;
    auto& value = document.root["actors"].At(index);
    value.Set("name", doc::Value{actor.name});
    for (const auto& [key, vector] : {std::pair{"position", &actor.position}, {"rotation", &actor.rotation}, {"scale", &actor.scale}}) {
        auto array = doc::Value::MakeSeq(); for (auto number : *vector) array.Push(doc::Value{number});
        value["transform"].Set(key, std::move(array));
    }
    return Commit(std::move(document));
}
Status DocumentModel::AddActor() {
    if (!IsValid() || !IsScene()) return Error("editor.scene.required", "Open a valid scene first", m_path, "Open a document with schema alice/scene/1");
    auto document = m_document;
    auto& actors = document.root["actors"];
    if (actors.IsNull()) actors = doc::Value::MakeSeq();
    auto value = doc::Value::MakeMap();
    const auto current = Actors();
    usize suffix = 1; std::string name;
    for (;;) {
        name = "Actor" + std::to_string(suffix++);
        bool used = false; for (const auto& actor : current) if (actor.name == name) used = true;
        if (!used) break;
    }
    value.Set("name", doc::Value{name}); actors.Push(std::move(value));
    return Commit(std::move(document));
}
Status DocumentModel::DeleteActor(usize index) {
    if (!IsValid() || !IsScene() || index >= Actors().size()) return Error("editor.actor.invalid",
        "Select an actor in a valid scene", m_path, "Select an existing actor before deleting");
    auto document = m_document; auto& actors = document.root["actors"].Items();
    actors.erase(actors.begin() + static_cast<isize>(index)); return Commit(std::move(document));
}
} // namespace alice::editor
