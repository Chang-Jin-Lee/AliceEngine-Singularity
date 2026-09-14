// SPDX-License-Identifier: MIT
#include "Writer.h"

#include "Foundation/StringUtil.h"

#include <algorithm>

namespace alice::doc {
namespace {

/// 평문 스칼라로 썼을 때 파서가 다른 타입으로 읽어버릴 문자열인지.
bool LooksLikeOtherType(std::string_view s) noexcept {
    if (s == "null" || s == "Null" || s == "NULL" || s == "~") return true;
    if (s == "true" || s == "True" || s == "TRUE")   return true;
    if (s == "false" || s == "False" || s == "FALSE") return true;
    i64 i = 0;
    if (ParseI64(s, i)) return true;
    f64 f = 0.0;
    if (ParseF64(s, f)) return true;
    return false;
}

void AppendIndent(std::string& out, u32 level, u32 width) {
    out.append(static_cast<usize>(level) * width, ' ');
}

std::string YamlQuoteDouble(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (const char ch : s) {
        switch (ch) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\t': out += "\\t";  break;
            case '\r': out += "\\r";  break;
            default:   out += ch;     break;
        }
    }
    out += '"';
    return out;
}

/// 컴팩트 한 줄로 써도 되는 시퀀스인가(벡터·색상처럼 짧은 숫자 배열).
bool IsCompactableSeq(const Value& v, const YamlWriteOptions& opt) {
    if (!opt.compactNumberSeq || !v.IsSeq()) return false;
    if (v.Size() == 0 || v.Size() > opt.compactSeqMaxItems) return false;
    for (const Value& item : v.Items()) {
        if (!item.IsNumber() && !item.IsBool()) return false;
    }
    return true;
}

/// 한 줄로 접어도 되는 맵인가.
/// 조건: 짧고, 스칼라만 있고, 항목마다 주석이 없어야 한다(주석은 줄을 요구한다).
bool IsCompactableMap(const Value& v, const YamlWriteOptions& opt) {
    if (!opt.compactScalarMap || !v.IsMap()) return false;
    if (v.Size() == 0 || v.Size() > opt.compactMapMaxEntries) return false;
    for (const MapEntry& e : v.Entries()) {
        // 스칼라, 또는 한 줄로 접히는 짧은 숫자 배열까지 허용한다.
        // 벡터 인자(direction: [0, 1, 0])가 흔해서 이걸 막으면 대부분의 동작이 여러 줄이 된다.
        const bool ok = e.value.IsScalar() || IsCompactableSeq(e.value, opt);
        if (!ok) return false;
        if (!e.value.comments.empty() || !e.value.trailingComment.empty()) return false;
        if (e.value.IsString() && e.value.AsString().find('\n') != std::string::npos) return false;
    }
    return true;
}

std::string ScalarToYaml(const Value& v) {
    switch (v.GetKind()) {
        case Kind::Null:   return "null";
        case Kind::Bool:   return v.AsBool() ? "true" : "false";
        case Kind::Int:    { std::string o; detail::FormatAppend(o, v.AsInt()); return o; }
        case Kind::Float:  return FormatDouble(v.AsFloat());
        case Kind::String: {
            const std::string& s = v.AsString();
            return NeedsYamlQuoting(s) ? YamlQuoteDouble(s) : s;
        }
        default: break;
    }
    return "null";
}

class YamlWriter {
public:
    explicit YamlWriter(const YamlWriteOptions& opt) : m_opt(opt) {}

    std::string Run(const Value& root) {
        WriteComments(root.comments, 0);
        if (root.IsMap()) {
            WriteMapBody(root, 0);
        } else if (root.IsSeq()) {
            WriteSeqBody(root, 0);
        } else {
            m_out += ScalarToYaml(root);
            m_out += '\n';
        }
        return std::move(m_out);
    }

private:
    /// 값 앞에 붙어 있던 주석을 되돌려 쓴다. 빈 문자열은 빈 줄이다.
    ///
    /// 이 함수가 있어야 "읽고 → 고치고 → 되쓰기" 고리가 사람의 메모를 지우지 않는다.
    /// 주석을 잃는 포맷터는 아무도 쓰지 않는다.
    void WriteComments(const std::vector<std::string>& comments, u32 level) {
        for (const std::string& line : comments) {
            if (line.empty()) {
                m_out += '\n';
                continue;
            }
            AppendIndent(m_out, level, m_opt.indentWidth);
            m_out += "# ";
            m_out += line;
            m_out += '\n';
        }
    }

    void WriteTrailing(const Value& v) {
        if (v.trailingComment.empty()) return;
        m_out += "  # ";
        m_out += v.trailingComment;
    }

    void WriteMapBody(const Value& map, u32 level) {
        if (map.Size() == 0) {
            AppendIndent(m_out, level, m_opt.indentWidth);
            m_out += "{}\n";
            return;
        }
        for (const MapEntry& e : map.Entries()) {
            WriteComments(e.value.comments, level);
            AppendIndent(m_out, level, m_opt.indentWidth);
            m_out += NeedsYamlQuoting(e.key) ? YamlQuoteDouble(e.key) : e.key;
            m_out += ':';
            WriteValueAfterKey(e.value, level);
        }
    }

    /// "key:" 를 이미 쓴 상태에서 값을 이어 쓴다.
    void WriteValueAfterKey(const Value& v, u32 level) {
        if (v.IsMap()) {
            if (v.Size() == 0) { m_out += " {}"; WriteTrailing(v); m_out += '\n'; return; }
            // 짧은 인자 맵은 한 줄로 접는다.
            // `- audio.play: { sound: sounds/jump.sound.yaml }` 가 두 줄이 되면
            // 규칙 목록이 세 배로 길어지고 눈으로 훑을 수 없게 된다.
            if (IsCompactableMap(v, m_opt) && CurrentLineLength() + RenderCompactMap(v).size() + 1
                                                  <= m_opt.compactLineLimit) {
                m_out += ' ';
                m_out += RenderCompactMap(v);
                WriteTrailing(v);
                m_out += '\n';
                return;
            }
            WriteTrailing(v);
            m_out += '\n';
            WriteMapBody(v, level + 1);
            return;
        }
        if (v.IsSeq()) {
            if (v.Size() == 0) { m_out += " []"; WriteTrailing(v); m_out += '\n'; return; }
            if (IsCompactableSeq(v, m_opt)) {
                m_out += ' ';
                WriteCompactSeq(v);
                WriteTrailing(v);
                m_out += '\n';
                return;
            }
            WriteTrailing(v);
            m_out += '\n';
            WriteSeqBody(v, level + 1);
            return;
        }
        if (v.IsString() && m_opt.useBlockScalars &&
            v.AsString().find('\n') != std::string::npos) {
            WriteBlockScalar(v.AsString(), level + 1);
            return;
        }
        m_out += ' ';
        m_out += ScalarToYaml(v);
        WriteTrailing(v);
        m_out += '\n';
    }

    void WriteSeqBody(const Value& seq, u32 level) {
        for (const Value& item : seq.Items()) {
            WriteComments(item.comments, level);
            AppendIndent(m_out, level, m_opt.indentWidth);
            m_out += '-';

            if (item.IsMap() && item.Size() > 0) {
                if (IsCompactableMap(item, m_opt)) {
                    const std::string inline_ = RenderCompactMap(item);
                    if (level * m_opt.indentWidth + 2 + inline_.size() <= m_opt.compactLineLimit) {
                        m_out += ' ';
                        m_out += inline_;
                        WriteTrailing(item);
                        m_out += '\n';
                        continue;
                    }
                }
                // "- key: v" 로 첫 키를 같은 줄에 붙인다. 줄 수가 눈에 띄게 줄어든다.
                m_out += ' ';
                const Map& entries = item.Entries();
                for (usize i = 0; i < entries.size(); ++i) {
                    if (i > 0) {
                        WriteComments(entries[i].value.comments, level + 1);
                        AppendIndent(m_out, level + 1, m_opt.indentWidth);
                    }
                    m_out += NeedsYamlQuoting(entries[i].key) ? YamlQuoteDouble(entries[i].key)
                                                              : entries[i].key;
                    m_out += ':';
                    WriteValueAfterKey(entries[i].value, level + 1);
                }
                continue;
            }
            if (item.IsSeq() && item.Size() > 0) {
                if (IsCompactableSeq(item, m_opt)) {
                    m_out += ' ';
                    WriteCompactSeq(item);
                    m_out += '\n';
                    continue;
                }
                m_out += '\n';
                WriteSeqBody(item, level + 1);
                continue;
            }
            if (item.IsMap()) { m_out += " {}\n"; continue; }
            if (item.IsSeq()) { m_out += " []\n"; continue; }

            if (item.IsString() && m_opt.useBlockScalars &&
                item.AsString().find('\n') != std::string::npos) {
                WriteBlockScalar(item.AsString(), level + 1);
                continue;
            }
            m_out += ' ';
            m_out += ScalarToYaml(item);
            m_out += '\n';
        }
    }

    /// 지금 쓰고 있는 줄의 길이. 한 줄로 접을지 판단할 때 쓴다.
    usize CurrentLineLength() const {
        const usize nl = m_out.rfind('\n');
        return nl == std::string::npos ? m_out.size() : m_out.size() - nl - 1;
    }

    std::string RenderCompactMap(const Value& map) {
        // 안쪽에 공백을 준다. `{ a: 1, b: 2 }` 가 `{a: 1, b: 2}` 보다 눈에 덜 빡빡하다.
        std::string out = "{ ";
        bool first = true;
        for (const MapEntry& e : map.Entries()) {
            if (!first) out += ", ";
            first = false;
            out += NeedsYamlQuoting(e.key) ? YamlQuoteDouble(e.key) : e.key;
            out += ": ";
            if (e.value.IsSeq()) {
                out += '[';
                for (usize i = 0; i < e.value.Size(); ++i) {
                    if (i) out += ", ";
                    out += ScalarToYaml(e.value.At(i));
                }
                out += ']';
            } else {
                out += ScalarToYaml(e.value);
            }
        }
        out += " }";
        return out;
    }

    void WriteCompactSeq(const Value& seq) {
        m_out += '[';
        for (usize i = 0; i < seq.Size(); ++i) {
            if (i) m_out += ", ";
            m_out += ScalarToYaml(seq.At(i));
        }
        m_out += ']';
    }

    void WriteBlockScalar(const std::string& text, u32 level) {
        // 끝의 줄바꿈 하나는 clip 규칙이 알아서 복원하므로 지시자를 붙이지 않는다.
        // 그 밖의 경우는 '-'(strip) 또는 '+'(keep)를 명시해야 왕복이 깨지지 않는다.
        usize trailing = 0;
        while (trailing < text.size() && text[text.size() - 1 - trailing] == '\n') ++trailing;

        if (trailing == 0)      m_out += " |-\n";
        else if (trailing == 1) m_out += " |\n";
        else                    m_out += " |+\n";

        const std::string body = text.substr(0, text.size() - trailing);
        usize start = 0;
        while (true) {
            const usize nl = body.find('\n', start);
            const std::string_view lineText =
                (nl == std::string::npos) ? std::string_view(body).substr(start)
                                          : std::string_view(body).substr(start, nl - start);
            if (!lineText.empty()) {
                AppendIndent(m_out, level, m_opt.indentWidth);
                m_out += lineText;
            }
            m_out += '\n';
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
        // keep(+) 인 경우 남은 줄바꿈을 그대로 남긴다.
        for (usize i = 1; i < trailing; ++i) m_out += '\n';
    }

    const YamlWriteOptions& m_opt;
    std::string             m_out;
};

class JsonWriter {
public:
    explicit JsonWriter(const JsonWriteOptions& opt) : m_opt(opt) {}

    std::string Run(const Value& root) {
        Write(root, 0);
        if (m_opt.pretty) m_out += '\n';
        return std::move(m_out);
    }

private:
    void Newline(u32 level) {
        if (!m_opt.pretty) return;
        m_out += '\n';
        AppendIndent(m_out, level, m_opt.indentWidth);
    }

    void Write(const Value& v, u32 level) {
        switch (v.GetKind()) {
            case Kind::Null:   m_out += "null"; return;
            case Kind::Bool:   m_out += v.AsBool() ? "true" : "false"; return;
            case Kind::Int:    detail::FormatAppend(m_out, v.AsInt()); return;
            case Kind::Float:  m_out += FormatDouble(v.AsFloat()); return;
            case Kind::String: m_out += JsonQuote(v.AsString()); return;

            case Kind::Seq: {
                if (v.Size() == 0) { m_out += "[]"; return; }
                m_out += '[';
                for (usize i = 0; i < v.Size(); ++i) {
                    if (i) m_out += ',';
                    Newline(level + 1);
                    Write(v.At(i), level + 1);
                }
                Newline(level);
                m_out += ']';
                return;
            }

            case Kind::Map: {
                if (v.Size() == 0) { m_out += "{}"; return; }

                std::vector<const MapEntry*> entries;
                entries.reserve(v.Size());
                for (const MapEntry& e : v.Entries()) entries.push_back(&e);
                if (m_opt.sortKeys) {
                    std::sort(entries.begin(), entries.end(),
                              [](const MapEntry* a, const MapEntry* b) { return a->key < b->key; });
                }

                m_out += '{';
                for (usize i = 0; i < entries.size(); ++i) {
                    if (i) m_out += ',';
                    Newline(level + 1);
                    m_out += JsonQuote(entries[i]->key);
                    m_out += ':';
                    if (m_opt.pretty) m_out += ' ';
                    Write(entries[i]->value, level + 1);
                }
                Newline(level);
                m_out += '}';
                return;
            }
        }
    }

    const JsonWriteOptions& m_opt;
    std::string             m_out;
};

} // namespace

bool NeedsYamlQuoting(std::string_view s) noexcept {
    if (s.empty()) return true;
    if (LooksLikeOtherType(s)) return true;

    // 앞뒤 공백은 평문으로 쓰면 사라진다.
    if (s.front() == ' ' || s.back() == ' ') return true;
    if (s.front() == '\t' || s.back() == '\t') return true;

    // 첫 글자가 구조 문자면 파서가 다른 것으로 읽는다.
    switch (s.front()) {
        case '-': case '?': case ':': case ',': case '[': case ']':
        case '{': case '}': case '#': case '&': case '*': case '!':
        case '|': case '>': case '\'': case '"': case '%': case '@': case '`':
            return true;
        default: break;
    }

    for (usize i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '\n' || c == '\r' || c == '\t') return true;
        // ": " 는 키 구분자로 읽힌다.
        if (c == ':' && (i + 1 == s.size() || s[i + 1] == ' ')) return true;
        // " #" 는 주석 시작으로 읽힌다.
        if (c == '#' && i > 0 && s[i - 1] == ' ') return true;
    }
    return false;
}

std::string ToYaml(const Value& value, const YamlWriteOptions& options) {
    YamlWriter writer(options);
    return writer.Run(value);
}

std::string ToJson(const Value& value, const JsonWriteOptions& options) {
    JsonWriter writer(options);
    return writer.Run(value);
}

} // namespace alice::doc
