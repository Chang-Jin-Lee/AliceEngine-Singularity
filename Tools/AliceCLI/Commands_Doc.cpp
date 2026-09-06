// SPDX-License-Identifier: MIT
// 문서를 다루는 명령들: check / fmt / convert / new
#include "Commands.h"

#include "Doc/Document.h"
#include "Doc/Writer.h"
#include "Foundation/FileSystem.h"
#include "Foundation/Log.h"
#include "Foundation/Profiler.h"
#include "Foundation/Time.h"
#include "Schema/Registry.h"
#include "Schema/SchemaBuilder.h"
#include "Schema/Validator.h"
#include "Verbs/SemanticCheck.h"
#include "Verbs/Verb.h"

#include <algorithm>
#include <cstdio>

namespace alice::cli {
namespace {

using doc::Document;
using doc::Value;

/// 인자로 준 경로들을 실제 문서 파일 목록으로 편다.
/// 디렉터리를 주면 안에 있는 .yaml / .yml / .json 을 전부 모은다.
std::vector<std::string> CollectDocuments(const std::vector<std::string>& inputs) {
    std::vector<std::string> files;
    for (const std::string& input : inputs) {
        if (fs::IsDirectory(input)) {
            for (const char* ext : {".yaml", ".yml", ".json"}) {
                std::vector<std::string> found = fs::ListFiles(input, ext, true);
                files.insert(files.end(), found.begin(), found.end());
            }
        } else if (fs::IsFile(input)) {
            files.push_back(fs::Normalize(input));
        }
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    return files;
}

/// 한 문서를 끝까지 검사한다. 단계마다 다음으로 넘어갈지 판단한다.
struct FileReport {
    std::string   path;
    std::string   schemaId;
    bool          parsed = false;
    DiagnosticBag diagnostics;

    bool Ok() const { return parsed && !diagnostics.HasErrors(); }
};

FileReport CheckOne(const std::string& path, bool skipSemantics) {
    ALICE_PROFILE_ZONE("cli.check.file");

    FileReport report;
    report.path = path;

    Document document;
    report.parsed = doc::LoadDocument(path, document, report.diagnostics);
    if (!report.parsed) return report;

    report.schemaId = document.SchemaId();

    const schema::Registry& schemas = schema::Registry::Global();

    if (report.schemaId.empty()) {
        // schema: 가 없으면 무엇으로 검증할지 알 수 없다. 이건 경고가 아니라 에러다 —
        // 이 한 줄이 AI 가 문서를 이해하는 유일한 실마리이기 때문이다.
        Diagnostic& d = report.diagnostics.Error(
            "doc.schema.missing",
            "문서에 schema: 필드가 없다");
        d.file = path;
        d.mark = Mark{1, 1, 0};
        d.hint = Fmt("첫 줄에 'schema: <id>' 를 적어라. 쓸 수 있는 id: {}",
                     Join(schemas.Ids(), ", "));
        return report;
    }

    schema::SchemaPtr found = schemas.Find(report.schemaId);
    if (!found) {
        Diagnostic& d = report.diagnostics.Error(
            "doc.schema.unknown",
            Fmt("'{}' 는 이 엔진이 모르는 스키마다", report.schemaId));
        d.file = path;
        d.mark = document.root["schema"].mark;

        const std::string suggestion = ClosestMatch(report.schemaId, schemas.Ids());
        if (!suggestion.empty()) d.hint = Fmt("'{}' 을(를) 뜻했는가? ", suggestion);
        d.hint += "`alice schema list` 로 전체 목록을 볼 수 있다";
        return report;
    }

    // 1단계: 구조 검증.
    schema::Validate(document.root, *found, schemas, report.diagnostics);

    // 2단계: 동사와 조건식 검증. 구조가 깨진 문서에서는 잡음만 늘어나므로 건너뛴다.
    if (!skipSemantics && !report.diagnostics.HasErrors()) {
        verbs::CheckDocumentSemantics(document.root, verbs::VerbRegistry::Global(),
                                      schemas, report.diagnostics, path);
    }

    report.diagnostics.SetFileIfEmpty(path);
    report.diagnostics.AttachSnippets(document.text);
    return report;
}

std::string ReportsToJson(const std::vector<FileReport>& reports,
                          usize errors, usize warnings, f64 elapsedMs) {
    std::string out = "{\"ok\":";
    out += (errors == 0 ? "true" : "false");
    out += ",\"files\":";
    detail::FormatAppend(out, static_cast<u64>(reports.size()));
    out += ",\"errors\":";
    detail::FormatAppend(out, static_cast<u64>(errors));
    out += ",\"warnings\":";
    detail::FormatAppend(out, static_cast<u64>(warnings));
    out += ",\"elapsedMs\":";
    out += FormatDouble(elapsedMs);
    out += ",\"results\":[";

    for (usize i = 0; i < reports.size(); ++i) {
        const FileReport& r = reports[i];
        if (i) out += ',';
        out += "{\"path\":";     out += JsonQuote(r.path);
        out += ",\"schema\":";   out += JsonQuote(r.schemaId);
        out += ",\"ok\":";       out += (r.Ok() ? "true" : "false");
        out += ",\"diagnostics\":[";
        const std::vector<Diagnostic>& items = r.diagnostics.Items();
        for (usize j = 0; j < items.size(); ++j) {
            if (j) out += ',';
            out += items[j].ToJson();
        }
        out += "]}";
    }
    out += "]}";
    return out;
}

/// 스키마에서 문서 뼈대를 만든다.
/// 필수 필드 + 기본값이 있는 필드를 넣고, 값이 없으면 타입을 알려주는 자리표시자를 둔다.
Value ScaffoldFromSchema(const schema::Schema& s, const schema::Registry& registry,
                         u32 depth = 0);

Value PlaceholderFor(const schema::Schema& s, const schema::Registry& registry, u32 depth) {
    using schema::Type;

    if (!s.examples.empty()) return s.examples.front();

    switch (s.type) {
        case Type::Bool:   return Value{false};
        case Type::Int:    return Value{static_cast<i64>(0)};
        case Type::Float:
        case Type::Number: return Value{0.0};
        case Type::String:
            if (!s.enumValues.empty()) return Value{s.enumValues.front()};
            switch (s.format) {
                case schema::TextFormat::AssetPath: return Value{"path/to/asset"};
                case schema::TextFormat::ColorHex:  return Value{"#ffffff"};
                case schema::TextFormat::VerbId:    return Value{"area.action"};
                case schema::TextFormat::Expression: return Value{"true"};
                default: return Value{"TODO"};
            }
        case Type::Seq: {
            Value seq = Value::MakeSeq();
            if (s.items && s.minItems && *s.minItems > 0) {
                for (usize i = 0; i < *s.minItems; ++i) {
                    seq.Push(PlaceholderFor(*s.items, registry, depth + 1));
                }
            }
            return seq;
        }
        case Type::Map:
            return ScaffoldFromSchema(s, registry, depth + 1);
        case Type::Ref: {
            schema::SchemaPtr target = registry.Find(s.refId);
            return target ? PlaceholderFor(*target, registry, depth + 1) : Value{};
        }
        case Type::Union:
            return s.options.empty() ? Value{}
                                     : PlaceholderFor(*s.options.front(), registry, depth + 1);
        default:
            return Value{};
    }
}

Value ScaffoldFromSchema(const schema::Schema& s, const schema::Registry& registry, u32 depth) {
    Value out = Value::MakeMap();
    if (depth > 6) return out;   // 재귀 스키마(actor.children)에서 멈춘다

    for (const schema::Field& field : s.fields) {
        const bool wanted = field.required || !field.defaultValue.IsNull();
        if (!wanted) continue;
        if (!field.deprecatedBy.empty()) continue;

        if (!field.defaultValue.IsNull()) {
            out.Set(field.name, field.defaultValue);
        } else if (field.type) {
            out.Set(field.name, PlaceholderFor(*field.type, registry, depth));
        } else {
            out.Set(field.name, Value{});
        }
    }
    return out;
}

/// 사람이 읽을 밀리초. 소수 2자리면 충분하고, 그 이상은 잡음이다.
std::string RoundedMs(f64 ms) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", ms);
    return buffer;
}

} // namespace

// ── check ──────────────────────────────────────────────────────────────────
int RunCheck(const Args& args) {
    std::vector<std::string> inputs = args.Positional();
    if (inputs.empty()) inputs.push_back(".");

    const std::vector<std::string> files = CollectDocuments(inputs);
    if (files.empty()) {
        if (Output().json) {
            PrintJson("{\"ok\":true,\"files\":0,\"errors\":0,\"warnings\":0,\"results\":[]}");
        } else {
            PrintLine(Yellow("검사할 문서를 찾지 못했다."));
            PrintLine(Dim("  .yaml / .yml / .json 파일이 있는 경로를 주거나 디렉터리를 지정하라."));
        }
        return 0;
    }

    const bool skipSemantics = args.Has("no-semantics");
    Stopwatch sw;

    std::vector<FileReport> reports;
    reports.reserve(files.size());
    usize errors = 0;
    usize warnings = 0;

    for (const std::string& file : files) {
        FileReport report = CheckOne(file, skipSemantics);
        errors   += report.diagnostics.ErrorCount();
        warnings += report.diagnostics.WarningCount();
        reports.push_back(std::move(report));
    }

    const f64 elapsedMs = sw.ElapsedMillis();

    if (Output().json) {
        PrintJson(ReportsToJson(reports, errors, warnings, elapsedMs));
        return errors == 0 ? 0 : 1;
    }

    for (const FileReport& r : reports) {
        if (r.diagnostics.Empty()) continue;
        PrintLine();
        PrintLine(Bold(r.path) + (r.schemaId.empty() ? "" : Dim("  [" + r.schemaId + "]")));
        for (const Diagnostic& d : r.diagnostics.Items()) {
            const std::string head = d.ToLine();
            PrintLine("  " + (d.severity == Severity::Error ? Red(head)
                              : d.severity == Severity::Warning ? Yellow(head) : Dim(head)));
            if (!d.snippet.empty()) {
                PrintLine("    " + Dim("| ") + d.snippet);
                if (d.mark.column > 0) {
                    PrintLine("    " + Dim("| ") + std::string(d.mark.column - 1, ' ') + Cyan("^"));
                }
            }
            if (!d.hint.empty()) PrintLine("    " + Green("= " + d.hint));
        }
    }

    PrintLine();
    std::string summary;
    detail::FormatAppend(summary, static_cast<u64>(files.size()));
    summary += "개 문서 · ";
    detail::FormatAppend(summary, static_cast<u64>(errors));
    summary += "개 오류 · ";
    detail::FormatAppend(summary, static_cast<u64>(warnings));
    summary += "개 경고  (";
    summary += RoundedMs(elapsedMs);
    summary += " ms)";
    PrintLine(errors == 0 ? Green("  " + summary) : Red("  " + summary));

    return errors == 0 ? 0 : 1;
}

// ── fmt ────────────────────────────────────────────────────────────────────
int RunFormat(const Args& args) {
    const std::vector<std::string> files = CollectDocuments(
        args.Positional().empty() ? std::vector<std::string>{"."} : args.Positional());

    const bool check = args.Has("check");   // 쓰지 않고 달라진 것만 보고
    usize changed = 0;
    std::vector<std::string> changedFiles;

    for (const std::string& file : files) {
        DiagnosticBag bag;
        Document document;
        if (!doc::LoadDocument(file, document, bag)) {
            PrintError(bag.ToPretty());
            continue;
        }

        const std::string formatted = doc::SerializeDocument(document);
        if (formatted == document.text) continue;

        ++changed;
        changedFiles.push_back(file);
        if (!check) {
            Status s = fs::WriteTextFile(file, formatted);
            if (s.IsErr()) PrintError(s.Error().ToLine());
        }
    }

    if (Output().json) {
        std::string out = "{\"changed\":";
        detail::FormatAppend(out, static_cast<u64>(changed));
        out += ",\"checked\":";
        detail::FormatAppend(out, static_cast<u64>(files.size()));
        out += ",\"files\":[";
        for (usize i = 0; i < changedFiles.size(); ++i) {
            if (i) out += ',';
            out += JsonQuote(changedFiles[i]);
        }
        out += "]}";
        PrintJson(out);
    } else {
        for (const std::string& f : changedFiles) {
            PrintLine((check ? Yellow("  다름  ") : Green("  정리  ")) + f);
        }
        std::string summary = "  ";
        detail::FormatAppend(summary, static_cast<u64>(files.size()));
        summary += "개 중 ";
        detail::FormatAppend(summary, static_cast<u64>(changed));
        summary += check ? "개가 정리되지 않았다" : "개를 정리했다";
        PrintLine(summary);
    }

    // --check 는 CI 용이다. 정리되지 않은 파일이 있으면 실패로 본다.
    return (check && changed > 0) ? 1 : 0;
}

// ── convert ────────────────────────────────────────────────────────────────
int RunConvert(const Args& args) {
    if (args.Positional().empty()) {
        PrintError("사용법: alice convert <입력> [출력]");
        PrintError("  출력을 생략하면 표준 출력으로 나간다.");
        return 2;
    }

    const std::string input  = args.Positional()[0];
    const std::string output = args.Positional().size() > 1 ? args.Positional()[1] : std::string{};

    DiagnosticBag bag;
    Document document;
    if (!doc::LoadDocument(input, document, bag)) {
        if (Output().json) PrintJson(bag.ToJsonObject());
        else               PrintError(bag.ToPretty());
        return 1;
    }

    doc::Syntax target = doc::Syntax::Auto;
    const std::string to = args.Get("to");
    if (to == "json")      target = doc::Syntax::Json;
    else if (to == "yaml") target = doc::Syntax::Yaml;
    else if (!output.empty()) target = doc::DetectSyntax(output, {});
    else target = (document.syntax == doc::Syntax::Json) ? doc::Syntax::Yaml : doc::Syntax::Json;

    const std::string text = doc::SerializeDocument(document, target);

    if (output.empty()) {
        // json 모드가 아니어도 변환 결과는 항상 표준 출력으로 나가야 한다.
        std::fwrite(text.data(), 1, text.size(), stdout);
        return 0;
    }

    Status s = fs::WriteTextFile(output, text);
    if (s.IsErr()) {
        PrintError(s.Error().ToLine());
        return 1;
    }
    PrintLine(Green("  변환  ") + input + Dim("  →  ") + output);
    return 0;
}

// ── new ────────────────────────────────────────────────────────────────────
int RunNew(const Args& args) {
    const schema::Registry& registry = schema::Registry::Global();

    if (args.Positional().empty()) {
        PrintError("사용법: alice new <타입> [이름] [-o <파일>]");
        PrintError("  타입은 스키마 id 의 짧은 이름이다. 예) actor, scene, material, behavior");
        PrintError("  `alice schema list` 로 전체 목록을 볼 수 있다.");
        return 2;
    }

    const std::string shortName = args.Positional()[0];
    const std::string docName   = args.Positional().size() > 1 ? args.Positional()[1] : "Untitled";

    // "actor" → "alice/actor/1" 로 넓혀 찾는다. 정확한 id 를 줘도 된다.
    schema::SchemaPtr found = registry.Find(shortName);
    if (!found) found = registry.FindLatest("alice/" + shortName);
    if (!found) found = registry.FindLatest(shortName);

    if (!found) {
        std::vector<std::string> shortNames;
        for (const std::string& id : registry.Ids()) {
            std::string base;
            u32 version = 0;
            doc::Document::SplitSchemaId(id, base, version);
            const usize slash = base.find_last_of('/');
            shortNames.push_back(slash == std::string::npos ? base : base.substr(slash + 1));
        }
        std::sort(shortNames.begin(), shortNames.end());
        shortNames.erase(std::unique(shortNames.begin(), shortNames.end()), shortNames.end());

        PrintError(Fmt("'{}' 는 모르는 문서 타입이다.", shortName));
        const std::string suggestion = ClosestMatch(shortName, shortNames);
        if (!suggestion.empty()) PrintError(Fmt("  '{}' 을(를) 뜻했는가?", suggestion));
        PrintError("  쓸 수 있는 타입: " + Join(shortNames, ", "));
        return 2;
    }

    Value scaffold = ScaffoldFromSchema(*found, registry);
    scaffold.Set("schema", Value{found->id});
    if (scaffold.Has("name")) scaffold.Set("name", Value{docName});

    // schema 를 맨 앞으로 올린다. 문서를 여는 사람과 AI 가 첫 줄에서 정체를 알아야 한다.
    Value ordered = Value::MakeMap();
    ordered.Set("schema", Value{found->id});
    for (const doc::MapEntry& e : scaffold.Entries()) {
        if (e.key != "schema") ordered.Set(e.key, e.value);
    }

    std::string text;
    text += "# " + (found->title.empty() ? found->id : found->title) + "\n";
    if (!found->description.empty()) text += "# " + found->description + "\n";
    text += "# 스키마: " + found->id + "\n";
    text += "# 필드 설명: alice schema show " + found->id + "\n";
    if (!found->pitfalls.empty()) {
        text += "#\n# 흔한 실수:\n";
        for (const schema::Schema::Pitfall& p : found->pitfalls) {
            text += "#   " + p.why + "\n";
        }
    }
    text += "\n";
    text += doc::ToYaml(ordered);

    const std::string outputPath = args.Get("o", args.Get("output"));
    if (outputPath.empty()) {
        std::fwrite(text.data(), 1, text.size(), stdout);
        return 0;
    }

    if (fs::Exists(outputPath) && !args.Has("force")) {
        PrintError(Fmt("'{}' 이(가) 이미 있다. 덮어쓰려면 --force 를 붙여라.", outputPath));
        return 1;
    }

    Status s = fs::WriteTextFile(outputPath, text);
    if (s.IsErr()) {
        PrintError(s.Error().ToLine());
        return 1;
    }
    PrintLine(Green("  생성  ") + outputPath + Dim("  [" + found->id + "]"));
    return 0;
}

} // namespace alice::cli
