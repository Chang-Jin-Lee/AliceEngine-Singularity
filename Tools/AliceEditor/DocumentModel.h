// SPDX-License-Identifier: MIT
// Keep edits, diagnostics and persistence independent of window callbacks.
#pragma once
#include "Doc/Document.h"
#include <array>

namespace alice::editor {
struct Actor {
    std::string name;
    std::array<f64, 3> position{0, 0, 0}, rotation{0, 0, 0}, scale{1, 1, 1};
};

Status AtomicSave(const std::string& path, std::string_view text, std::string_view expected);
Status AtomicCreate(const std::string& path, std::string_view text);

class DocumentModel {
public:
    Status Open(const std::string& path);
    Status CreateScene(const std::string& path, const std::string& name);
    void SetText(std::string text);
    bool Validate();
    Status Save();
    bool Undo();
    bool Redo();
    bool IsDirty() const { return m_text != m_saved; }
    bool IsScene() const { return m_parsed && m_document.SchemaId() == "alice/scene/1"; }
    bool IsValid() const { return m_parsed && !m_diagnostics.HasErrors(); }
    const std::string& Text() const { return m_text; }
    const std::string& Path() const { return m_path; }
    const DiagnosticBag& Diagnostics() const { return m_diagnostics; }
    const doc::Document& Document() const { return m_document; }
    std::vector<Actor> Actors() const;
    Status EditActor(usize index, const Actor& actor);
    Status AddActor();
    Status DeleteActor(usize index);
private:
    Status Commit(doc::Document document);
    void Remember();
    std::string m_path, m_text, m_saved;
    doc::Document m_document;
    DiagnosticBag m_diagnostics;
    bool m_parsed = false;
    std::vector<std::string> m_undo, m_redo;
};
} // namespace alice::editor
