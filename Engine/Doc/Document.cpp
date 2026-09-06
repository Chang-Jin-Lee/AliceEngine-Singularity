// SPDX-License-Identifier: MIT
#include "Document.h"

#include "Foundation/FileSystem.h"
#include "Foundation/StringUtil.h"
#include "JsonParser.h"
#include "Writer.h"
#include "YamlParser.h"

namespace alice::doc {

const char* ToString(Syntax s) noexcept {
    switch (s) {
        case Syntax::Auto: return "auto";
        case Syntax::Yaml: return "yaml";
        case Syntax::Json: return "json";
    }
    return "yaml";
}

std::string Document::SchemaId() const {
    const Value* v = root.Find("schema");
    return (v && v->IsString()) ? v->AsString() : std::string{};
}

bool Document::SplitSchemaId(std::string_view id, std::string& outBase, u32& outVersion) {
    outBase.clear();
    outVersion = 0;
    if (id.empty()) return false;

    const usize slash = id.find_last_of('/');
    if (slash == std::string_view::npos) { outBase = std::string(id); return false; }

    i64 version = 0;
    if (!ParseI64(id.substr(slash + 1), version) || version < 0) {
        outBase = std::string(id);
        return false;
    }
    outBase    = std::string(id.substr(0, slash));
    outVersion = static_cast<u32>(version);
    return true;
}

Syntax DetectSyntax(std::string_view path, std::string_view text) {
    const std::string ext = fs::Extension(std::string(path));
    if (ext == ".json" || ext == ".jsonc") return Syntax::Json;
    if (ext == ".yaml" || ext == ".yml")   return Syntax::Yaml;

    // 확장자가 없거나 낯설면 첫 실질 문자로 본다.
    const std::string_view t = TrimLeft(text);
    if (!t.empty() && (t.front() == '{' || t.front() == '[')) return Syntax::Json;
    return Syntax::Yaml;
}

bool ParseDocument(std::string_view text,
                   Syntax syntax,
                   std::string sourceName,
                   Document& outDocument,
                   DiagnosticBag& diagnostics) {
    if (syntax == Syntax::Auto) syntax = DetectSyntax(sourceName, text);

    outDocument.path   = sourceName;
    outDocument.text   = std::string(text);
    outDocument.syntax = syntax;
    outDocument.root   = Value{};

    const usize before = diagnostics.Size();
    bool ok = (syntax == Syntax::Json)
                  ? ParseJson(text, outDocument.root, diagnostics)
                  : ParseYaml(text, outDocument.root, diagnostics);

    // 파서는 경로를 모른다. 여기서 채운다.
    for (usize i = before; i < diagnostics.Items().size(); ++i) {
        Diagnostic& d = diagnostics.Items()[i];
        if (d.file.empty()) d.file = outDocument.path;
    }
    return ok;
}

bool LoadDocument(const std::string& path,
                  Document& outDocument,
                  DiagnosticBag& diagnostics,
                  Syntax syntax) {
    Result<std::string> text = fs::ReadTextFile(path);
    if (text.IsErr()) {
        diagnostics.Add(text.Error());
        return false;
    }
    return ParseDocument(*text, syntax, path, outDocument, diagnostics);
}

std::string SerializeDocument(const Document& document, Syntax syntax) {
    if (syntax == Syntax::Auto) syntax = document.syntax;
    return (syntax == Syntax::Json) ? ToJson(document.root) : ToYaml(document.root);
}

Status SaveDocument(const Document& document, const std::string& path, Syntax syntax) {
    if (syntax == Syntax::Auto) syntax = DetectSyntax(path, {});
    return fs::WriteTextFile(path, SerializeDocument(document, syntax));
}

} // namespace alice::doc
