// SPDX-License-Identifier: MIT
// 엔진 자신에 대해 답하는 명령들: schema / verbs / explain / doctor / version / help
//
// 이 명령들이 있어야 AI 가 문서를 읽지 않고도 엔진을 쓸 수 있다.
// 문서는 낡지만 이 출력은 코드에서 직접 나오므로 절대 낡지 않는다.
#include "Commands.h"

#include "Doc/Document.h"
#include "Doc/Writer.h"
#include "Foundation/FileSystem.h"
#include "Foundation/StringUtil.h"
#include "RHI/RHIBackend.h"
#include "Schema/JsonSchemaEmitter.h"
#include "Schema/Registry.h"
#include "Verbs/Expression.h"
#include "Verbs/Verb.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace alice::cli {
namespace {

using doc::Value;

/// PATH 에 그 명령이 있는지. 엔진 내장 AI 대화창이 무엇에 붙을 수 있는지 판단한다.
bool CommandExists(const char* name) {
#if ALICE_PLATFORM_WINDOWS
    const std::string probe = std::string("where ") + name + " >nul 2>&1";
#else
    const std::string probe = std::string("command -v ") + name + " >/dev/null 2>&1";
#endif
    return std::system(probe.c_str()) == 0;
}

std::string ShortNameOf(const std::string& schemaId) {
    std::string base;
    u32 version = 0;
    doc::Document::SplitSchemaId(schemaId, base, version);
    const usize slash = base.find_last_of('/');
    return slash == std::string::npos ? base : base.substr(slash + 1);
}

std::string TypeLabel(const schema::Schema& s, const schema::Registry& registry) {
    using schema::Type;
    switch (s.type) {
        case Type::Seq: {
            if (!s.items) return "list";
            std::string inner = TypeLabel(*s.items, registry);
            if (s.minItems && s.maxItems && *s.minItems == *s.maxItems) {
                std::string out = "[" + inner;
                out += " x";
                detail::FormatAppend(out, static_cast<u64>(*s.minItems));
                out += "]";
                return out;
            }
            return "[" + inner + "]";
        }
        case Type::Ref: return ShortNameOf(s.refId);
        case Type::Union: {
            std::vector<std::string> parts;
            for (const schema::SchemaPtr& o : s.options) {
                if (o) parts.push_back(TypeLabel(*o, registry));
            }
            return Join(parts, " | ");
        }
        case Type::String:
            if (!s.enumValues.empty()) return Join(s.enumValues, " | ");
            if (s.format != schema::TextFormat::None) return schema::ToString(s.format);
            return "string";
        default:
            return schema::ToString(s.type);
    }
}

void PrintSchemaHuman(const schema::Schema& s, const schema::Registry& registry) {
    PrintLine();
    PrintLine(Bold(s.title.empty() ? s.id : s.title) + Dim("  " + s.id));
    if (!s.description.empty()) {
        PrintLine("  " + s.description);
    }

    if (!s.fields.empty()) {
        PrintLine();
        PrintLine(Dim("  필드"));
        usize widest = 0;
        for (const schema::Field& f : s.fields) widest = std::max(widest, f.name.size());

        for (const schema::Field& f : s.fields) {
            std::string line = "    ";
            line += f.required ? Red("*") : " ";
            line += Cyan(f.name);
            line.append(widest - f.name.size() + 2, ' ');
            line += Dim(f.type ? TypeLabel(*f.type, registry) : "any");
            PrintLine(line);

            if (!f.description.empty()) {
                PrintLine("        " + f.description);
            }
            if (!f.defaultValue.IsNull()) {
                PrintLine("        " + Dim("기본값: " + f.defaultValue.ToScalarText()));
            }
            if (!f.aliases.empty()) {
                PrintLine("        " + Dim("옛 이름: " + Join(f.aliases, ", ")));
            }
            if (!f.deprecatedBy.empty()) {
                PrintLine("        " + Yellow("폐기됨 → " + f.deprecatedBy));
            }
        }
        PrintLine();
        PrintLine(Dim("    * 표시가 필수 필드"));
    }

    if (!s.examples.empty()) {
        PrintLine();
        PrintLine(Dim("  예시"));
        for (const Value& e : s.examples) {
            const std::string yaml = doc::ToYaml(e);
            for (std::string_view line : Split(yaml, '\n')) {
                if (!line.empty()) PrintLine("    " + std::string(line));
            }
        }
    }

    if (!s.pitfalls.empty()) {
        PrintLine();
        PrintLine(Dim("  흔한 실수"));
        for (const schema::Schema::Pitfall& p : s.pitfalls) {
            PrintLine("    " + Red("✗ " + p.wrong));
            PrintLine("    " + Green("✓ " + p.right));
            PrintLine("      " + Dim(p.why));
            PrintLine();
        }
    }
}

void PrintVerbHuman(const verbs::Verb& v, const schema::Registry& registry) {
    PrintLine();
    PrintLine(Bold(v.id) + Dim("  [" + Join(v.tags, ", ") + "]"));
    PrintLine("  " + v.summary);
    if (!v.description.empty()) {
        PrintLine();
        PrintLine("  " + v.description);
    }

    if (!v.deterministic) {
        PrintLine();
        PrintLine("  " + Yellow("비결정적: 같은 입력이어도 결과가 다를 수 있다 (리플레이 주의)"));
    }

    if (v.args && !v.args->fields.empty()) {
        PrintLine();
        PrintLine(Dim("  인자"));
        usize widest = 0;
        for (const schema::Field& f : v.args->fields) widest = std::max(widest, f.name.size());
        for (const schema::Field& f : v.args->fields) {
            std::string line = "    ";
            line += f.required ? Red("*") : " ";
            line += Cyan(f.name);
            line.append(widest - f.name.size() + 2, ' ');
            line += Dim(f.type ? TypeLabel(*f.type, registry) : "any");
            PrintLine(line);
            if (!f.description.empty()) PrintLine("        " + f.description);
        }
    }

    if (!v.examples.empty()) {
        PrintLine();
        PrintLine(Dim("  예시"));
        for (const Value& e : v.examples) {
            for (std::string_view line : Split(doc::ToYaml(e), '\n')) {
                if (!line.empty()) PrintLine("    - " + std::string(line));
            }
        }
    }
}

// ── 진단 코드 설명표 ───────────────────────────────────────────────────────
// 자주 나오는 코드만 손으로 적는다. 나머지는 진단 자체의 hint 가 충분히 설명한다.
// 여기 적는 기준: **왜 이 규칙이 있는지**를 알아야 고칠 수 있는 것들.
const CodeExplanation kExplanations[] = {
    {"doc.parse.tab_indent",
     "들여쓰기에 탭을 썼다",
     "YAML 명세가 탭 들여쓰기를 금지한다. 탭 폭이 편집기마다 달라 문서의 구조가 사람마다 다르게 보이기 때문이다.",
     "root:\n\tchild: 1",
     "root:\n  child: 1"},

    {"doc.parse.value_eaten_by_comment",
     "값이 주석으로 해석되어 사라졌다",
     "YAML 에서 공백 뒤의 '#' 는 주석 시작이다. 16진 색상을 그대로 적으면 값 전체가 사라지고 조용히 null 이 된다. 이 엔진에서 가장 흔한 콘텐츠 버그라 파서가 따로 잡아낸다.",
     "tint: #ff8800",
     "tint: \"#ff8800\""},

    {"doc.parse.duplicate_key",
     "같은 키가 두 번 나왔다",
     "대부분의 YAML 파서는 뒤엣것으로 조용히 덮어쓴다. 그러면 앞의 설정이 왜 안 먹는지 아무도 모른다. 이 엔진은 에러로 만든다.",
     "name: A\nname: B",
     "name: B"},

    {"doc.parse.anchor_unsupported",
     "앵커(&)와 별칭(*)은 지원하지 않는다",
     "앵커는 문서를 사람도 AI 도 읽기 어렵게 만든다. 값 재사용은 별도 문서로 빼고 경로로 참조한다.",
     "base: &common\n  a: 1\ncopy: *common",
     "base: common.yaml  # 별도 파일로 분리"},

    {"doc.schema.missing",
     "문서에 schema: 필드가 없다",
     "이 한 줄이 문서의 정체를 밝힌다. 없으면 무엇으로 검증할지, 어떤 자동완성을 줄지, 런타임이 어떻게 읽을지 아무것도 정해지지 않는다.",
     "name: Player",
     "schema: alice/actor/1\nname: Player"},

    {"schema.unknown_field",
     "스키마에 없는 필드다",
     "모르는 필드를 통과시키면 오타가 영원히 발견되지 않는다. 진단에 가장 가까운 이름과 전체 후보가 함께 나온다.",
     "positon: [0, 0, 0]",
     "position: [0, 0, 0]"},

    {"schema.missing_field",
     "필수 필드가 없다",
     "필수 필드는 그것 없이는 런타임이 동작을 정할 수 없는 값이다. 기본값으로 때울 수 있었다면 애초에 필수가 아니다.",
     "components:\n  mesh: {}",
     "components:\n  mesh:\n    asset: meshes/x.mesh"},

    {"schema.type_mismatch",
     "타입이 맞지 않는다",
     "따옴표 하나로 숫자가 문자열이 되는 사고가 가장 흔하다. 진단이 그 경우를 따로 짚어 준다.",
     "health: \"100\"",
     "health: 100"},

    {"verb.unknown",
     "없는 동사다",
     "콘텐츠가 부를 수 있는 것은 레지스트리에 등록된 동사뿐이다. 목록이 유한하다는 점이 이 엔진의 설계 의도다.",
     "- audio.paly: { sound: x }",
     "- audio.play: { sound: x }"},

    {"verb.action_multiple_keys",
     "동작 하나에 키가 여럿이다",
     "각 동작은 동사 하나다. 여러 동작을 하려면 목록에 항목을 더 만든다. 이 규칙이 있어야 실행 순서가 문서 순서와 정확히 일치한다.",
     "- anim.trigger: { parameter: Jump }\n  wait: { seconds: 1 }",
     "- anim.trigger: { parameter: Jump }\n- wait: { seconds: 1 }"},

    {"expr.assignment",
     "식에서 대입할 수 없다",
     "조건식은 부작용이 없어야 한다. 그래야 규칙을 어떤 순서로 평가해도 결과가 같다. 값을 바꾸는 일은 do: 의 동사가 한다.",
     "when: health = 0",
     "when: health == 0"},

    {"expr.unknown_name",
     "식에서 쓸 수 없는 이름이다",
     "식이 읽을 수 있는 상태는 심볼표에 등록된 것과 문서가 선언한 변수뿐이다. `alice verbs --symbols` 로 전체를 볼 수 있다.",
     "when: physics.grouned",
     "when: physics.grounded"},

    {"expr.function_not_called",
     "함수를 괄호 없이 썼다",
     "input.pressed 는 값이 아니라 함수다. 어떤 행동을 물어보는지 인자로 줘야 한다.",
     "when: input.pressed",
     "when: input.pressed(\"Jump\")"},

    {"perf.budget.exceeded",
     "존이 프레임 예산을 넘었다",
     "예산은 프로젝트 문서의 budgets 에서 선언한다. 초과는 에러가 아니라 신호다 — 이 로그가 있어야 '느려졌다'는 말이 기계가 추적할 수 있는 사건이 된다.",
     "budgets: {}",
     "budgets:\n  Frame: 16.6\n  Render: 8.0"},

    {"rhi.leak.detected",
     "종료 시점에 GPU 리소스가 살아 있다",
     "Null 백엔드는 누수를 이름과 함께 보고한다. 진짜 백엔드에서는 조용히 메모리만 늘어나므로 CI 에서 Null 로 먼저 잡는다.",
     "device->CreateBuffer(...)  // Destroy 없음",
     "device->CreateBuffer(...); ... device->Destroy(handle);"},
};

} // namespace

const CodeExplanation* FindExplanation(std::string_view code) {
    for (const CodeExplanation& e : kExplanations) {
        if (code == e.code) return &e;
    }
    return nullptr;
}

const CodeExplanation* AllExplanations(usize& outCount) {
    outCount = sizeof(kExplanations) / sizeof(kExplanations[0]);
    return kExplanations;
}

// ── schema ─────────────────────────────────────────────────────────────────
int RunSchema(const Args& args) {
    const schema::Registry& registry = schema::Registry::Global();
    const std::string sub = args.Positional().empty() ? "list" : args.Positional()[0];

    if (sub == "list") {
        if (Output().json) {
            std::string out = "{\"count\":";
            detail::FormatAppend(out, static_cast<u64>(registry.Size()));
            out += ",\"schemas\":[";
            const std::vector<schema::SchemaPtr> all = registry.All();
            for (usize i = 0; i < all.size(); ++i) {
                if (i) out += ',';
                out += "{\"id\":";        out += JsonQuote(all[i]->id);
                out += ",\"short\":";     out += JsonQuote(ShortNameOf(all[i]->id));
                out += ",\"title\":";     out += JsonQuote(all[i]->title);
                out += ",\"description\":"; out += JsonQuote(all[i]->description);
                out += ",\"fields\":";    detail::FormatAppend(out, static_cast<u64>(all[i]->fields.size()));
                out += '}';
            }
            out += "]}";
            PrintJson(out);
            return 0;
        }

        PrintLine();
        PrintLine(Bold("이 엔진이 아는 문서 타입"));
        PrintLine();
        for (const schema::SchemaPtr& s : registry.All()) {
            std::string line = "  " + Cyan(s->id);
            line.append(s->id.size() < 34 ? 34 - s->id.size() : 2, ' ');
            line += s->title.empty() ? "" : s->title;
            PrintLine(line);
            if (!s->description.empty()) {
                PrintLine("    " + Dim(s->description));
            }
        }
        PrintLine();
        PrintLine(Dim("  자세히: alice schema show <id>"));
        return 0;
    }

    if (sub == "show") {
        if (args.Positional().size() < 2) {
            PrintError("사용법: alice schema show <id>");
            return 2;
        }
        const std::string id = args.Positional()[1];
        schema::SchemaPtr found = registry.Find(id);
        if (!found) found = registry.FindLatest("alice/" + id);
        if (!found) found = registry.FindLatest(id);

        if (!found) {
            PrintError(Fmt("'{}' 를 찾을 수 없다.", id));
            const std::string suggestion = ClosestMatch(id, registry.Ids());
            if (!suggestion.empty()) PrintError(Fmt("  '{}' 을(를) 뜻했는가?", suggestion));
            return 1;
        }

        if (Output().json) {
            PrintJson(schema::EmitJsonSchema(*found, registry));
        } else {
            PrintSchemaHuman(*found, registry);
        }
        return 0;
    }

    if (sub == "emit") {
        const std::string outputDir =
            args.Positional().size() > 1 ? args.Positional()[1] : args.Get("o", "Schemas");

        Status s = schema::WriteSchemaFiles(registry, outputDir);
        if (s.IsErr()) {
            PrintError(s.Error().ToLine());
            return 1;
        }
        if (Output().json) {
            std::string out = "{\"ok\":true,\"directory\":";
            out += JsonQuote(outputDir);
            out += ",\"count\":";
            detail::FormatAppend(out, static_cast<u64>(registry.Size()));
            out += "}";
            PrintJson(out);
        } else {
            std::string line = "  " + Green("생성") + "  ";
            detail::FormatAppend(line, static_cast<u64>(registry.Size()));
            line += "개 JSON Schema → " + outputDir + "/";
            PrintLine(line);
            PrintLine(Dim("  에디터에 물리려면 .vscode/settings.json 의 yaml.schemas 에 등록하라."));
        }
        return 0;
    }

    PrintError(Fmt("알 수 없는 하위 명령: {}", sub));
    PrintError("  쓸 수 있는 것: list, show, emit");
    return 2;
}

// ── verbs ──────────────────────────────────────────────────────────────────
int RunVerbs(const Args& args) {
    const verbs::VerbRegistry& registry = verbs::VerbRegistry::Global();
    const schema::Registry&    schemas  = schema::Registry::Global();

    // --symbols : 조건식에서 쓸 수 있는 이름들
    if (args.Has("symbols")) {
        const verbs::SymbolTable& table = verbs::SymbolTable::Core();

        if (Output().json) {
            std::string out = "{\"symbols\":[";
            bool first = true;
            for (const std::string& name : table.Names()) {
                const verbs::SymbolTable::Symbol* s = table.Find(name);
                if (!s) continue;
                if (!first) out += ',';
                first = false;
                out += "{\"name\":";     out += JsonQuote(s->name);
                out += ",\"kind\":";     out += JsonQuote(s->kind == verbs::SymbolTable::Kind::Function
                                                              ? "function" : "property");
                out += ",\"type\":";     out += JsonQuote(s->type);
                out += ",\"summary\":";  out += JsonQuote(s->summary);
                if (s->kind == verbs::SymbolTable::Kind::Function) {
                    out += ",\"minArgs\":"; detail::FormatAppend(out, static_cast<u64>(s->minArgs));
                    out += ",\"maxArgs\":"; detail::FormatAppend(out, static_cast<u64>(s->maxArgs));
                }
                out += '}';
            }
            out += "]}";
            PrintJson(out);
            return 0;
        }

        PrintLine();
        PrintLine(Bold("조건식(when:)에서 쓸 수 있는 이름"));
        PrintLine();
        for (const std::string& name : table.Names()) {
            const verbs::SymbolTable::Symbol* s = table.Find(name);
            if (!s) continue;
            std::string line = "  " + Cyan(s->name);
            if (s->kind == verbs::SymbolTable::Kind::Function) line += "(…)";
            line.append(line.size() < 34 ? 34 - line.size() : 2, ' ');
            line += Dim(s->type);
            PrintLine(line);
            if (!s->summary.empty()) PrintLine("      " + s->summary);
        }
        return 0;
    }

    // 특정 동사 하나
    if (!args.Positional().empty()) {
        const std::string id = args.Positional()[0];
        const verbs::Verb* verb = registry.Find(id);
        if (!verb) {
            PrintError(Fmt("'{}' 는 없는 동사다.", id));
            const std::string suggestion = ClosestMatch(id, registry.Ids());
            if (!suggestion.empty()) PrintError(Fmt("  '{}' 을(를) 뜻했는가?", suggestion));
            return 1;
        }
        if (Output().json) {
            // 하나만 담은 목록으로 낸다. 형식이 목록일 때와 같아야 파서가 하나면 된다.
            verbs::VerbRegistry single;
            single.Register(*verb);
            PrintJson(single.ToJson(schemas));
        } else {
            PrintVerbHuman(*verb, schemas);
        }
        return 0;
    }

    // 전체 목록
    if (Output().json) {
        PrintJson(registry.ToJson(schemas));
        return 0;
    }

    const std::string tag = args.Get("tag");
    PrintLine();
    PrintLine(Bold("콘텐츠가 쓸 수 있는 동작"));
    PrintLine();

    std::string lastTag;
    for (const verbs::Verb* v : registry.ByTag(tag)) {
        const std::string primary = v->tags.empty() ? "" : v->tags.front();
        if (primary != lastTag) {
            PrintLine();
            PrintLine("  " + Dim(primary));
            lastTag = primary;
        }
        std::string line = "    " + Cyan(v->id);
        line.append(v->id.size() < 26 ? 26 - v->id.size() : 2, ' ');
        line += v->summary;
        if (!v->deterministic) line += Yellow("  (비결정적)");
        PrintLine(line);
    }
    PrintLine();
    PrintLine(Dim("  자세히: alice verbs <id>      조건식 이름들: alice verbs --symbols"));
    return 0;
}

// ── explain ────────────────────────────────────────────────────────────────
int RunExplain(const Args& args) {
    usize count = 0;
    const CodeExplanation* all = AllExplanations(count);

    if (args.Positional().empty() || args.Has("list")) {
        if (Output().json) {
            std::string out = "{\"codes\":[";
            for (usize i = 0; i < count; ++i) {
                if (i) out += ',';
                out += "{\"code\":";      out += JsonQuote(all[i].code);
                out += ",\"summary\":";   out += JsonQuote(all[i].summary);
                out += ",\"why\":";       out += JsonQuote(all[i].why);
                out += ",\"wrong\":";     out += JsonQuote(all[i].wrong);
                out += ",\"right\":";     out += JsonQuote(all[i].right);
                out += '}';
            }
            out += "]}";
            PrintJson(out);
            return 0;
        }
        PrintLine();
        PrintLine(Bold("설명이 있는 진단 코드"));
        PrintLine();
        for (usize i = 0; i < count; ++i) {
            std::string line = "  " + Cyan(all[i].code);
            line.append(std::strlen(all[i].code) < 34 ? 34 - std::strlen(all[i].code) : 2, ' ');
            line += all[i].summary;
            PrintLine(line);
        }
        PrintLine();
        PrintLine(Dim("  자세히: alice explain <코드>"));
        return 0;
    }

    const std::string code = args.Positional()[0];
    const CodeExplanation* found = FindExplanation(code);
    if (!found) {
        PrintError(Fmt("'{}' 에 대한 설명이 아직 없다.", code));
        std::vector<std::string> codes;
        for (usize i = 0; i < count; ++i) codes.emplace_back(all[i].code);
        const std::string suggestion = ClosestMatch(code, codes);
        if (!suggestion.empty()) PrintError(Fmt("  '{}' 을(를) 뜻했는가?", suggestion));
        PrintError("  진단 자체의 hint 에 대개 고치는 법이 들어 있다.");
        return 1;
    }

    if (Output().json) {
        std::string out = "{\"code\":";
        out += JsonQuote(found->code);
        out += ",\"summary\":"; out += JsonQuote(found->summary);
        out += ",\"why\":";     out += JsonQuote(found->why);
        out += ",\"wrong\":";   out += JsonQuote(found->wrong);
        out += ",\"right\":";   out += JsonQuote(found->right);
        out += '}';
        PrintJson(out);
        return 0;
    }

    PrintLine();
    PrintLine(Bold(found->code));
    PrintLine("  " + std::string(found->summary));
    PrintLine();
    PrintLine("  " + std::string(found->why));
    PrintLine();
    PrintLine(Dim("  이렇게 쓰면 안 된다"));
    for (std::string_view line : Split(found->wrong, '\n')) {
        PrintLine("    " + Red(std::string(line)));
    }
    PrintLine();
    PrintLine(Dim("  이렇게 쓴다"));
    for (std::string_view line : Split(found->right, '\n')) {
        PrintLine("    " + Green(std::string(line)));
    }
    PrintLine();
    return 0;
}

// ── doctor ─────────────────────────────────────────────────────────────────
int RunDoctor(const Args& args) {
    ALICE_UNUSED(args);
    rhi::LinkAllBackends();

    std::vector<std::string> backendNames;
    for (const rhi::Backend b : rhi::AvailableBackends()) {
        backendNames.emplace_back(rhi::ToString(b));
    }

    // 엔진 안 AI 대화창(요구 6)이 붙을 수 있는지 미리 확인한다.
    const bool hasClaude = CommandExists("claude");
    const bool hasCodex  = CommandExists("codex");

    if (Output().json) {
        std::string out = "{\"engine\":\"AliceEngine-Singularity\",\"version\":";
        out += JsonQuote(ALICE_VERSION_STRING);
        out += ",\"platform\":";
#if ALICE_PLATFORM_WINDOWS
        out += JsonQuote("windows");
#elif ALICE_PLATFORM_MACOS
        out += JsonQuote("macos");
#elif ALICE_PLATFORM_IOS
        out += JsonQuote("ios");
#elif ALICE_PLATFORM_ANDROID
        out += JsonQuote("android");
#else
        out += JsonQuote("linux");
#endif
        out += ",\"schemas\":";
        detail::FormatAppend(out, static_cast<u64>(schema::Registry::Global().Size()));
        out += ",\"verbs\":";
        detail::FormatAppend(out, static_cast<u64>(verbs::VerbRegistry::Global().Size()));
        out += ",\"symbols\":";
        detail::FormatAppend(out, static_cast<u64>(verbs::SymbolTable::Core().Size()));
        out += ",\"backends\":[";
        for (usize i = 0; i < backendNames.size(); ++i) {
            if (i) out += ',';
            out += JsonQuote(backendNames[i]);
        }
        out += "],\"aiCli\":{\"claude\":";
        out += hasClaude ? "true" : "false";
        out += ",\"codex\":";
        out += hasCodex ? "true" : "false";
        out += "}}";
        PrintJson(out);
        return 0;
    }

    PrintLine();
    PrintLine(Bold("AliceEngine-Singularity ") + Dim(ALICE_VERSION_STRING));
    PrintLine();
    PrintLine("  " + Cyan("플랫폼      ") +
#if ALICE_PLATFORM_WINDOWS
              "windows"
#elif ALICE_PLATFORM_MACOS
              "macos"
#elif ALICE_PLATFORM_IOS
              "ios"
#elif ALICE_PLATFORM_ANDROID
              "android"
#else
              "linux"
#endif
    );

    std::string counts;
    detail::FormatAppend(counts, static_cast<u64>(schema::Registry::Global().Size()));
    counts += "개 문서 타입 · ";
    detail::FormatAppend(counts, static_cast<u64>(verbs::VerbRegistry::Global().Size()));
    counts += "개 동사 · ";
    detail::FormatAppend(counts, static_cast<u64>(verbs::SymbolTable::Core().Size()));
    counts += "개 식 심볼";
    PrintLine("  " + Cyan("콘텐츠 모델 ") + counts);

    PrintLine("  " + Cyan("렌더 백엔드 ") +
              (backendNames.empty() ? Red("없음") : Join(backendNames, ", ")));

    PrintLine("  " + Cyan("AI CLI      ") +
              std::string("claude ") + (hasClaude ? Green("있음") : Dim("없음")) +
              "  ·  codex " + (hasCodex ? Green("있음") : Dim("없음")));
    if (!hasClaude && !hasCodex) {
        PrintLine("              " + Dim("엔진 내장 대화창을 쓰려면 둘 중 하나가 PATH 에 있어야 한다."));
    }
    PrintLine();
    return 0;
}

// ── version / help ─────────────────────────────────────────────────────────
int RunVersion(const Args& args) {
    ALICE_UNUSED(args);
    if (Output().json) {
        std::string out = "{\"version\":";
        out += JsonQuote(ALICE_VERSION_STRING);
        out += ",\"engine\":\"AliceEngine-Singularity\"}";
        PrintJson(out);
    } else {
        PrintLine(std::string("AliceEngine-Singularity ") + ALICE_VERSION_STRING);
    }
    return 0;
}

int RunHelp(const Args& args) {
    ALICE_UNUSED(args);
    if (Output().json) {
        PrintJson("{\"commands\":[\"check\",\"fmt\",\"convert\",\"new\","
                  "\"schema\",\"verbs\",\"explain\",\"doctor\",\"version\",\"help\"]}");
        return 0;
    }

    PrintLine();
    PrintLine(Bold("alice") + Dim(" — AliceEngine-Singularity 콘텐츠 도구"));
    PrintLine();
    PrintLine(Dim("  콘텐츠"));
    PrintLine("    " + Cyan("check   ") + " <경로>...        문서를 검증한다 (스키마 · 동사 · 조건식)");
    PrintLine("    " + Cyan("fmt     ") + " <경로>...        문서를 정규화한다  --check 면 쓰지 않고 확인만");
    PrintLine("    " + Cyan("convert ") + " <입력> [출력]     YAML ↔ JSON       --to=json|yaml");
    PrintLine("    " + Cyan("new     ") + " <타입> [이름]     스키마에서 뼈대를 만든다  -o <파일>");
    PrintLine();
    PrintLine(Dim("  엔진에게 묻기"));
    PrintLine("    " + Cyan("schema  ") + " list|show|emit   문서 타입과 필드");
    PrintLine("    " + Cyan("verbs   ") + " [id]             콘텐츠가 쓸 수 있는 동작  --symbols 는 조건식 이름");
    PrintLine("    " + Cyan("explain ") + " <코드>            진단 코드가 왜 있는지");
    PrintLine("    " + Cyan("doctor  ") + "                  환경 점검");
    PrintLine();
    PrintLine(Dim("  공통 플래그"));
    PrintLine("    " + Green("--json") + "        모든 출력을 기계가 읽는 형태로. AI 는 이걸 쓴다");
    PrintLine("    --quiet       사람용 출력을 줄인다");
    PrintLine("    --no-color    색을 끈다");
    PrintLine();
    PrintLine(Dim("  종료 코드: 0 성공 · 1 검증 실패 · 2 사용법 오류"));
    PrintLine();
    return 0;
}

} // namespace alice::cli
