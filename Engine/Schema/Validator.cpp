// SPDX-License-Identifier: MIT
#include "Validator.h"

#include "Foundation/StringUtil.h"
#include "Registry.h"

#include <algorithm>

namespace alice::schema {
namespace {

using doc::Kind;

std::string JoinPath(const std::string& base, std::string_view key) {
    if (base.empty()) return std::string(key);
    std::string out = base;
    out += '.';
    out.append(key);
    return out;
}

std::string IndexPath(const std::string& base, usize index) {
    std::string out = base;
    out += '[';
    detail::FormatAppend(out, static_cast<u64>(index));
    out += ']';
    return out;
}

/// 스키마를 사람이 부를 이름. 진단 문장에 들어간다.
std::string NameOf(const Schema& s) {
    if (!s.title.empty()) return s.title;
    if (!s.id.empty())    return s.id;
    return ToString(s.type);
}

/// 후보 목록을 진단에 넣을 문자열로. 너무 길면 자른다.
std::string CandidateList(const std::vector<std::string>& names, usize limit = 12) {
    std::string out;
    const usize n = std::min(limit, names.size());
    for (usize i = 0; i < n; ++i) {
        if (i) out += ", ";
        out += names[i];
    }
    if (names.size() > n) {
        out += ", … (";
        detail::FormatAppend(out, static_cast<u64>(names.size() - n));
        out += "개 더)";
    }
    return out;
}

bool TypeMatches(const Value& v, Type t) {
    switch (t) {
        case Type::Any:    return true;
        case Type::Null:   return v.IsNull();
        case Type::Bool:   return v.IsBool();
        case Type::Int:    return v.IsInt();
        // 실수 자리에 정수를 쓰는 것은 허용한다. scale: 1 을 scale: 1.0 으로 쓰라고
        // 강요하면 문서가 지저분해지고 AI 가 자주 틀린다. 반대 방향은 허용하지 않는다.
        case Type::Float:  return v.IsFloat() || v.IsInt();
        case Type::Number: return v.IsNumber();
        case Type::String: return v.IsString();
        case Type::Seq:    return v.IsSeq();
        case Type::Map:    return v.IsMap();
        case Type::Ref:    return true;    // 참조를 푼 뒤 검사한다
        case Type::Union:  return true;    // 각 옵션에서 검사한다
    }
    return false;
}

class Validator {
public:
    Validator(const Registry& registry, DiagnosticBag& diags, const ValidateOptions& options)
        : m_registry(registry), m_diags(diags), m_options(options) {}

    void Check(const Value& value, const Schema& schema, const std::string& path) {
        if (Saturated()) return;

        // 참조를 먼저 푼다.
        if (schema.type == Type::Ref) {
            SchemaPtr target = m_registry.Find(schema.refId);
            if (!target) {
                Diagnostic& d = Error("schema.unresolved_ref",
                                      Fmt("참조한 스키마를 찾을 수 없다: {}", schema.refId));
                d.path = path;
                d.mark = value.mark;
                d.hint = Fmt("등록된 스키마: {}", CandidateList(m_registry.Ids()));
                return;
            }
            Check(value, *target, path);
            return;
        }

        if (schema.type == Type::Union) {
            CheckUnion(value, schema, path);
            return;
        }

        if (!TypeMatches(value, schema.type)) {
            Diagnostic& d = Error("schema.type_mismatch",
                                  Fmt("{} 를 기대했는데 {} 가 왔다",
                                         ToString(schema.type), doc::ToString(value.GetKind())));
            d.path = path;
            d.mark = value.mark;
            d.hint = TypeHint(schema, value);
            return;
        }

        switch (schema.type) {
            case Type::Map:    CheckMap(value, schema, path); break;
            case Type::Seq:    CheckSeq(value, schema, path); break;
            case Type::String: CheckString(value, schema, path); break;
            case Type::Int:
            case Type::Float:
            case Type::Number: CheckNumber(value, schema, path); break;
            default: break;
        }
    }

private:
    bool Saturated() const { return m_diags.Size() >= m_options.maxDiagnostics; }

    Diagnostic& Error(std::string code, std::string message) {
        return m_diags.Error(std::move(code), std::move(message));
    }
    Diagnostic& Warning(std::string code, std::string message) {
        return m_diags.Warning(std::move(code), std::move(message));
    }

    /// 타입이 어긋났을 때 "어떻게 고치나"를 만들어 준다.
    std::string TypeHint(const Schema& schema, const Value& value) {
        // 흔한 실수 먼저: 따옴표 때문에 숫자가 문자열이 된 경우.
        if (schema.type != Type::String && value.IsString()) {
            i64 i = 0;
            f64 f = 0.0;
            const std::string& s = value.AsString();
            if (ParseI64(s, i) || ParseF64(s, f)) {
                return Fmt("따옴표를 벗기면 숫자가 된다: {}", s);
            }
        }
        if (schema.type == Type::Seq && value.IsNumber()) {
            return "값 하나가 아니라 배열이 와야 한다. 예) [0, 0, 0]";
        }
        if (schema.type == Type::Map && value.IsNull()) {
            return "이 키에 값이 비어 있다. 하위 필드를 들여쓰기로 적거나 {} 를 쓰라";
        }
        if (!schema.examples.empty()) {
            return Fmt("예시: {}", schema.examples.front().ToScalarText());
        }
        return {};
    }

    void CheckUnion(const Value& value, const Schema& schema, const std::string& path) {
        // 각 옵션을 조용히(별도 가방에) 시도해서 하나라도 통과하면 성공.
        for (const SchemaPtr& option : schema.options) {
            if (!option) continue;
            DiagnosticBag scratch;
            Validator sub(m_registry, scratch, m_options);
            sub.Check(value, *option, path);
            if (!scratch.HasErrors()) return;
        }

        std::vector<std::string> shapes;
        for (const SchemaPtr& option : schema.options) {
            if (option) shapes.push_back(ToString(option->type));
        }

        Diagnostic& d = Error("schema.union_no_match",
                              Fmt("{} 가 허용되는 어떤 형태와도 맞지 않는다",
                                     doc::ToString(value.GetKind())));
        d.path = path;
        d.mark = value.mark;
        d.hint = schema.description.empty()
                     ? Fmt("허용되는 형태: {}", CandidateList(shapes))
                     : schema.description;
    }

    void CheckMap(const Value& value, const Schema& schema, const std::string& path) {
        // 1) 알 수 없는 필드
        const bool allowExtra = schema.additionalFields || m_options.allowUnknownFields;
        for (const doc::MapEntry& entry : value.Entries()) {
            if (Saturated()) return;

            const Field* field = schema.FindField(entry.key);
            if (field) continue;

            if (const Field* renamed = schema.FindFieldByAlias(entry.key)) {
                Diagnostic& d = Warning("schema.renamed_field",
                                        Fmt("'{}' 은(는) 옛 이름이다", entry.key));
                d.path = JoinPath(path, entry.key);
                d.mark = entry.value.mark;
                d.hint = Fmt("'{}' 으로 바꿔라", renamed->name);
                continue;
            }

            if (allowExtra) {
                if (schema.additionalFieldType) {
                    Check(entry.value, *schema.additionalFieldType, JoinPath(path, entry.key));
                }
                continue;
            }

            Diagnostic& d = Error("schema.unknown_field",
                                  Fmt("'{}' 은(는) {} 에 없는 필드다", entry.key, NameOf(schema)));
            d.path = JoinPath(path, entry.key);
            d.mark = entry.value.mark;

            const std::vector<std::string> names = schema.FieldNames();
            const std::string suggestion = ClosestMatch(entry.key, names);
            if (!suggestion.empty()) {
                d.hint = Fmt("'{}' 을(를) 뜻했는가? ", suggestion);
            }
            d.hint += Fmt("쓸 수 있는 필드: {}", CandidateList(names));
        }

        // 2) 필수 필드 누락
        for (const Field& field : schema.fields) {
            if (Saturated()) return;
            if (!field.required) continue;
            if (value.Has(field.name)) continue;

            Diagnostic& d = Error("schema.missing_field",
                                  Fmt("필수 필드 '{}' 가 없다", field.name));
            d.path = path.empty() ? field.name : path;
            d.mark = value.mark;
            d.hint = field.description.empty()
                         ? Fmt("'{}: <{}>' 를 추가하라", field.name, ToString(field.type ? field.type->type : Type::Any))
                         : Fmt("'{}: <{}>' 를 추가하라 — {}", field.name,
                                  ToString(field.type ? field.type->type : Type::Any),
                                  field.description);
        }

        // 3) 폐기된 필드
        if (m_options.warnDeprecated) {
            for (const Field& field : schema.fields) {
                if (field.deprecatedBy.empty()) continue;
                if (!value.Has(field.name)) continue;

                Diagnostic& d = Warning("schema.deprecated_field",
                                        Fmt("'{}' 는 폐기되었다", field.name));
                d.path = JoinPath(path, field.name);
                d.mark = value[field.name].mark;
                d.hint = Fmt("대신 '{}' 를 쓰라", field.deprecatedBy);
            }
        }

        // 4) 각 필드 검사
        for (const Field& field : schema.fields) {
            if (Saturated()) return;
            const Value* v = value.Find(field.name);
            if (!v || !field.type) continue;
            Check(*v, *field.type, JoinPath(path, field.name));
        }
    }

    void CheckSeq(const Value& value, const Schema& schema, const std::string& path) {
        if (schema.minItems && value.Size() < *schema.minItems) {
            Diagnostic& d = Error("schema.too_few_items",
                                  Fmt("항목이 {}개인데 최소 {}개가 필요하다",
                                         static_cast<u64>(value.Size()),
                                         static_cast<u64>(*schema.minItems)));
            d.path = path;
            d.mark = value.mark;
            if (schema.minItems == schema.maxItems) {
                d.hint = Fmt("길이가 정확히 {}이어야 한다", static_cast<u64>(*schema.minItems));
            }
        }
        if (schema.maxItems && value.Size() > *schema.maxItems) {
            Diagnostic& d = Error("schema.too_many_items",
                                  Fmt("항목이 {}개인데 최대 {}개까지다",
                                         static_cast<u64>(value.Size()),
                                         static_cast<u64>(*schema.maxItems)));
            d.path = path;
            d.mark = value.mark;
        }

        if (!schema.items) return;
        for (usize i = 0; i < value.Size(); ++i) {
            if (Saturated()) return;
            Check(value.At(i), *schema.items, IndexPath(path, i));
        }
    }

    void CheckString(const Value& value, const Schema& schema, const std::string& path) {
        const std::string& s = value.AsString();

        if (!schema.enumValues.empty()) {
            const bool ok = std::find(schema.enumValues.begin(), schema.enumValues.end(), s) !=
                            schema.enumValues.end();
            if (!ok) {
                Diagnostic& d = Error("schema.enum_mismatch",
                                      Fmt("'{}' 는 허용된 값이 아니다", s));
                d.path = path;
                d.mark = value.mark;
                const std::string suggestion = ClosestMatch(s, schema.enumValues);
                if (!suggestion.empty()) d.hint = Fmt("'{}' 을(를) 뜻했는가? ", suggestion);
                d.hint += Fmt("허용값: {}", CandidateList(schema.enumValues));
                return;
            }
        }

        if (schema.minLength && s.size() < *schema.minLength) {
            Diagnostic& d = Error("schema.string_too_short",
                                  Fmt("길이가 {}인데 최소 {}이어야 한다",
                                         static_cast<u64>(s.size()),
                                         static_cast<u64>(*schema.minLength)));
            d.path = path;
            d.mark = value.mark;
        }
        if (schema.maxLength && s.size() > *schema.maxLength) {
            Diagnostic& d = Error("schema.string_too_long",
                                  Fmt("길이가 {}인데 최대 {}까지다",
                                         static_cast<u64>(s.size()),
                                         static_cast<u64>(*schema.maxLength)));
            d.path = path;
            d.mark = value.mark;
        }

        if (schema.format != TextFormat::None) {
            std::string reason;
            if (!CheckFormat(schema.format, s, reason)) {
                Diagnostic& d = Error("schema.bad_format",
                                      Fmt("'{}' 는 {} 형식이 아니다", s, ToString(schema.format)));
                d.path = path;
                d.mark = value.mark;
                d.hint = reason;
            }
        }
    }

    void CheckNumber(const Value& value, const Schema& schema, const std::string& path) {
        const f64 n = value.AsFloat();
        if (schema.minValue && n < *schema.minValue) {
            Diagnostic& d = Error("schema.out_of_range",
                                  Fmt("{} 는 최솟값 {} 보다 작다",
                                         FormatDouble(n), FormatDouble(*schema.minValue)));
            d.path = path;
            d.mark = value.mark;
        }
        if (schema.maxValue && n > *schema.maxValue) {
            Diagnostic& d = Error("schema.out_of_range",
                                  Fmt("{} 는 최댓값 {} 보다 크다",
                                         FormatDouble(n), FormatDouble(*schema.maxValue)));
            d.path = path;
            d.mark = value.mark;
        }
    }

    const Registry&        m_registry;
    DiagnosticBag&         m_diags;
    const ValidateOptions& m_options;
};

} // namespace

bool CheckFormat(TextFormat format, std::string_view text, std::string& outReason) {
    outReason.clear();

    switch (format) {
        case TextFormat::None:
            return true;

        case TextFormat::Identifier: {
            if (text.empty()) { outReason = "비어 있다"; return false; }
            const char first = text.front();
            if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z') || first == '_')) {
                outReason = "영문자나 밑줄로 시작해야 한다";
                return false;
            }
            for (const char c : text) {
                const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || c == '_';
                if (!ok) {
                    outReason = Fmt("'{}' 는 식별자에 쓸 수 없다. 영숫자와 밑줄만 된다",
                                       std::string(1, c));
                    return false;
                }
            }
            return true;
        }

        case TextFormat::AssetPath: {
            if (text.empty()) { outReason = "비어 있다"; return false; }
            if (text.find('\\') != std::string_view::npos) {
                outReason = "역슬래시 대신 슬래시(/)를 쓰라. 경로는 플랫폼 중립이어야 한다";
                return false;
            }
            if (text.front() == '/') {
                outReason = "절대 경로를 쓸 수 없다. 프로젝트 루트 기준 상대 경로를 쓰라";
                return false;
            }
            if (text.find("..") != std::string_view::npos) {
                outReason = "'..' 로 프로젝트 밖을 가리킬 수 없다";
                return false;
            }
            return true;
        }

        case TextFormat::SchemaId: {
            // "<네임스페이스>/<이름>/<버전>"
            const usize slash = text.find_last_of('/');
            if (slash == std::string_view::npos) {
                outReason = "'<네임스페이스>/<이름>/<버전>' 형태여야 한다. 예) alice/actor/1";
                return false;
            }
            i64 version = 0;
            if (!ParseI64(text.substr(slash + 1), version) || version < 1) {
                outReason = "마지막 조각은 1 이상의 버전 번호여야 한다. 예) alice/actor/1";
                return false;
            }
            return true;
        }

        case TextFormat::VerbId: {
            if (text.find('.') == std::string_view::npos) {
                outReason = "'<영역>.<동작>' 형태여야 한다. 예) audio.play";
                return false;
            }
            for (const char c : text) {
                const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                                c == '.' || c == '_';
                if (!ok) {
                    outReason = "소문자, 숫자, 점, 밑줄만 쓸 수 있다";
                    return false;
                }
            }
            return true;
        }

        case TextFormat::ColorHex: {
            if (text.empty() || text.front() != '#') {
                outReason = "'#' 로 시작해야 한다. 예) \"#ff8800\"";
                return false;
            }
            const std::string_view digits = text.substr(1);
            if (digits.size() != 3 && digits.size() != 6 && digits.size() != 8) {
                outReason = "'#' 뒤에 3, 6, 8 자리 16진수가 와야 한다";
                return false;
            }
            for (const char c : digits) {
                const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                                 (c >= 'A' && c <= 'F');
                if (!hex) {
                    outReason = Fmt("'{}' 는 16진수가 아니다", std::string(1, c));
                    return false;
                }
            }
            return true;
        }

        case TextFormat::Expression:
            if (Trim(text).empty()) {
                outReason = "식이 비어 있다";
                return false;
            }
            return true;
    }
    return true;
}

bool Validate(const Value&    value,
              const Schema&   schema,
              const Registry& registry,
              DiagnosticBag&  diagnostics,
              const ValidateOptions& options,
              std::string_view rootPath) {
    const usize before = diagnostics.ErrorCount();
    Validator v(registry, diagnostics, options);
    v.Check(value, schema, std::string(rootPath));
    return diagnostics.ErrorCount() == before;
}

Value ApplyDefaults(const Value& value, const Schema& schema, const Registry& registry) {
    if (schema.type == Type::Ref) {
        SchemaPtr target = registry.Find(schema.refId);
        return target ? ApplyDefaults(value, *target, registry) : value;
    }

    if (schema.type == Type::Seq) {
        if (!value.IsSeq() || !schema.items) return value;
        Value out = Value::MakeSeq();
        out.mark = value.mark;
        for (usize i = 0; i < value.Size(); ++i) {
            out.Push(ApplyDefaults(value.At(i), *schema.items, registry));
        }
        return out;
    }

    if (schema.type != Type::Map) return value;
    if (!value.IsMap() && !value.IsNull()) return value;

    Value out = Value::MakeMap();
    out.mark = value.mark;

    // 스키마 순서를 따른다. 그래야 생성된 문서의 필드 순서가 항상 같다 —
    // diff 를 읽을 수 있게 만드는 조건이다.
    for (const Field& field : schema.fields) {
        const Value* present = value.IsMap() ? value.Find(field.name) : nullptr;
        if (present) {
            out.Set(field.name, field.type ? ApplyDefaults(*present, *field.type, registry)
                                           : *present);
        } else if (!field.defaultValue.IsNull()) {
            out.Set(field.name, field.defaultValue);
        }
    }

    // 스키마 밖 필드는 뒤에 붙여 보존한다. 알 수 없다고 버리면 데이터가 사라진다.
    if (value.IsMap()) {
        for (const doc::MapEntry& entry : value.Entries()) {
            if (!schema.FindField(entry.key)) out.Set(entry.key, entry.value);
        }
    }
    return out;
}

} // namespace alice::schema
