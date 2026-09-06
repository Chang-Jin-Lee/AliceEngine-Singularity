// SPDX-License-Identifier: MIT
#include "SemanticCheck.h"

#include "Foundation/StringUtil.h"
#include "Schema/Validator.h"

namespace alice::verbs {
namespace {

using doc::Kind;
using doc::MapEntry;
using doc::Value;

std::string JoinPath(std::string_view base, std::string_view key) {
    if (base.empty()) return std::string(key);
    std::string out(base);
    out += '.';
    out.append(key);
    return out;
}

std::string IndexPath(std::string_view base, usize index) {
    std::string out(base);
    out += '[';
    detail::FormatAppend(out, static_cast<u64>(index));
    out += ']';
    return out;
}

/// 문서가 선언한 지역 이름들을 모아 식 검사에 얹는다.
/// behavior 의 variables, animation 의 parameters 가 여기 해당한다.
void CollectLocals(const Value& root, SymbolTable& table) {
    if (const Value* vars = root.Find("variables")) {
        for (const MapEntry& e : vars->Entries()) table.AddLocal(e.key);
    }
    if (const Value* params = root.Find("parameters")) {
        for (const MapEntry& e : params->Entries()) table.AddLocal(e.key);
    }
}

class Walker {
public:
    Walker(const VerbRegistry& verbs, const schema::Registry& schemas,
           DiagnosticBag& diags, std::string_view path,
           const SemanticCheckOptions& options, const SymbolTable& symbols)
        : m_verbs(verbs), m_schemas(schemas), m_diags(diags),
          m_path(path), m_options(options), m_symbols(symbols) {}

    void Walk(const Value& value, const std::string& path) {
        if (value.IsMap()) {
            for (const MapEntry& e : value.Entries()) {
                const std::string childPath = JoinPath(path, e.key);

                if (e.key == "when" && e.value.IsString()) {
                    CheckWhen(e.value, childPath);
                    continue;
                }
                if (e.key == "do" && e.value.IsSeq()) {
                    for (usize i = 0; i < e.value.Size(); ++i) {
                        CheckAction(e.value.At(i), m_verbs, m_schemas, m_diags,
                                    IndexPath(childPath, i), m_options);
                    }
                    continue;
                }
                Walk(e.value, childPath);
            }
            return;
        }

        if (value.IsSeq()) {
            for (usize i = 0; i < value.Size(); ++i) {
                Walk(value.At(i), IndexPath(path, i));
            }
        }
    }

private:
    void CheckWhen(const Value& value, const std::string& path) {
        if (!m_options.checkExpressionSymbols) return;

        const usize before = m_diags.Size();
        ValidateExpression(value.AsString(), m_symbols, m_diags, value.mark, m_path);

        // 식 진단에는 문서 안 경로가 없다. 여기서 채워야 AI 가 어느 규칙인지 안다.
        for (usize i = before; i < m_diags.Items().size(); ++i) {
            Diagnostic& d = m_diags.Items()[i];
            if (d.path.empty()) d.path = path;
            if (d.file.empty()) d.file = std::string(m_path);
        }
    }

    const VerbRegistry&         m_verbs;
    const schema::Registry&     m_schemas;
    DiagnosticBag&              m_diags;
    std::string_view            m_path;
    const SemanticCheckOptions& m_options;
    const SymbolTable&          m_symbols;
};

} // namespace

bool CheckAction(const Value& action, const VerbRegistry& verbs,
                 const schema::Registry& schemas, DiagnosticBag& diagnostics,
                 std::string_view path, const SemanticCheckOptions& options) {
    const usize before = diagnostics.ErrorCount();

    if (!action.IsMap()) {
        Diagnostic& d = diagnostics.Error(
            "verb.action_not_map",
            Fmt("동작은 맵이어야 하는데 {} 가 왔다", doc::ToString(action.GetKind())));
        d.hint = "각 동작은 동사 하나를 키로 갖는다. 예)  - audio.play: { sound: ... }";
        d.path = std::string(path);
        d.mark = action.mark;
        return false;
    }

    if (action.Size() != 1) {
        Diagnostic& d = diagnostics.Error(
            "verb.action_multiple_keys",
            Fmt("동작 하나에 키가 {}개 있다", static_cast<u64>(action.Size())));
        d.hint = "동작 하나에는 동사 하나만 온다. 여러 개를 하려면 목록에 항목을 더 만들어라";
        d.path = std::string(path);
        d.mark = action.mark;
        return false;
    }

    const MapEntry& entry  = action.Entries().front();
    const std::string& id  = entry.key;
    const Verb*     verb   = verbs.Find(id);

    if (!verb) {
        Diagnostic& d = diagnostics.Error("verb.unknown",
                                          Fmt("'{}' 는 없는 동사다", id));
        d.path = std::string(path);
        d.mark = entry.value.mark;

        const std::vector<std::string> ids = verbs.Ids();
        const std::string suggestion = ClosestMatch(id, ids);
        if (!suggestion.empty()) {
            d.hint = Fmt("'{}' 을(를) 뜻했는가? ", suggestion);
        }
        d.hint += "`alice verbs` 로 전체 목록을 볼 수 있다";
        return false;
    }

    if (options.checkVerbArguments && verb->args) {
        // 인자가 없는 동작은 빈 맵으로 본다. `- anim.trigger:` 처럼 쓸 수 있게.
        const Value  empty  = Value::MakeMap();
        const Value& args   = entry.value.IsNull() ? empty : entry.value;

        schema::ValidateOptions vo;
        const usize beforeArgs = diagnostics.Size();
        schema::Validate(args, *verb->args, schemas, diagnostics, vo, path);

        // 인자 진단에 동사 이름을 얹어 준다. "unknown_field" 만 보면 어느 동사인지 모른다.
        for (usize i = beforeArgs; i < diagnostics.Items().size(); ++i) {
            Diagnostic& d = diagnostics.Items()[i];
            if (d.message.find(id) == std::string::npos) {
                d.message = Fmt("{} 의 인자: {}", id, d.message);
            }
        }
    }

    return diagnostics.ErrorCount() == before;
}

bool CheckDocumentSemantics(const Value& root, const VerbRegistry& verbs,
                            const schema::Registry& schemas, DiagnosticBag& diagnostics,
                            std::string_view documentPath,
                            const SemanticCheckOptions& options) {
    const usize before = diagnostics.ErrorCount();

    // 문서가 선언한 지역 이름을 코어 심볼표 위에 얹는다.
    SymbolTable symbols = SymbolTable::Core();
    CollectLocals(root, symbols);

    Walker walker(verbs, schemas, diagnostics, documentPath, options, symbols);
    walker.Walk(root, {});

    return diagnostics.ErrorCount() == before;
}

} // namespace alice::verbs
