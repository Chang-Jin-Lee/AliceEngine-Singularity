// SPDX-License-Identifier: MIT
#include "JsonParser.h"

#include "Foundation/StringUtil.h"

namespace alice::doc {
namespace {

class JsonParser {
public:
    JsonParser(std::string_view text, DiagnosticBag& diags, const JsonParseOptions& opts)
        : m_text(text), m_diags(diags), m_opts(opts) {}

    bool Parse(Value& out) {
        SkipTrivia();
        if (AtEnd()) {
            Error("doc.json.empty", "빈 문서다", "최소한 {} 또는 [] 는 있어야 한다");
            return false;
        }
        out = ParseValue(0);
        SkipTrivia();
        if (!AtEnd()) {
            Error("doc.json.trailing", Fmt("최상위 값 뒤에 남은 내용이 있다: '{}'", std::string(1, Peek())),
                  "JSON 문서에는 최상위 값이 하나만 온다");
        }
        return !m_diags.HasErrors();
    }

private:
    bool AtEnd() const noexcept { return m_pos >= m_text.size(); }
    char Peek() const noexcept { return m_pos < m_text.size() ? m_text[m_pos] : '\0'; }
    char PeekAt(usize k) const noexcept {
        return (m_pos + k) < m_text.size() ? m_text[m_pos + k] : '\0';
    }

    void Advance() noexcept {
        if (m_pos >= m_text.size()) return;
        if (m_text[m_pos] == '\n') { ++m_line; m_lineStart = m_pos + 1; }
        ++m_pos;
    }

    Mark Here() const noexcept {
        return Mark{m_line, static_cast<u32>(m_pos - m_lineStart) + 1, static_cast<u32>(m_pos)};
    }

    Diagnostic& Error(std::string code, std::string message, std::string hint = {}) {
        Diagnostic& d = m_diags.Error(std::move(code), std::move(message));
        d.hint = std::move(hint);
        d.mark = Here();
        return d;
    }

    void SkipTrivia() {
        while (!AtEnd()) {
            const char c = Peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { Advance(); continue; }
            if (m_opts.allowComments && c == '/') {
                if (PeekAt(1) == '/') {
                    while (!AtEnd() && Peek() != '\n') Advance();
                    continue;
                }
                if (PeekAt(1) == '*') {
                    const Mark start = Here();
                    Advance(); Advance();
                    bool closed = false;
                    while (!AtEnd()) {
                        if (Peek() == '*' && PeekAt(1) == '/') { Advance(); Advance(); closed = true; break; }
                        Advance();
                    }
                    if (!closed) {
                        Diagnostic& d = m_diags.Error("doc.json.unterminated_comment",
                                                      "블록 주석이 닫히지 않았다");
                        d.mark = start;
                    }
                    continue;
                }
            }
            break;
        }
    }

    Value ParseValue(u32 depth) {
        if (depth > m_opts.maxDepth) {
            Error("doc.json.too_deep", Fmt("중첩이 너무 깊다 (한계 {})", m_opts.maxDepth));
            return Value{};
        }
        SkipTrivia();
        if (AtEnd()) {
            Error("doc.json.unexpected_end", "값이 와야 할 자리에서 문서가 끝났다");
            return Value{};
        }

        const Mark mark = Here();
        Value v;
        switch (Peek()) {
            case '{': v = ParseObject(depth); break;
            case '[': v = ParseArray(depth);  break;
            case '"': v = Value{ParseString()}; break;
            case 't': v = ParseLiteral("true",  Value{true});  break;
            case 'f': v = ParseLiteral("false", Value{false}); break;
            case 'n': v = ParseLiteral("null",  Value{});      break;
            default:  v = ParseNumber(); break;
        }
        v.mark = mark;
        return v;
    }

    Value ParseLiteral(std::string_view word, Value result) {
        if (m_text.compare(m_pos, word.size(), word) == 0) {
            for (usize i = 0; i < word.size(); ++i) Advance();
            return result;
        }
        Error("doc.json.bad_literal", Fmt("'{}' 이(가) 와야 한다", word),
              "true / false / null 은 소문자로만 쓴다");
        Advance();
        return Value{};
    }

    Value ParseNumber() {
        const usize start = m_pos;
        if (Peek() == '-' || Peek() == '+') Advance();

        bool isFloat = false;
        while (!AtEnd()) {
            const char c = Peek();
            if (c >= '0' && c <= '9') { Advance(); continue; }
            if (c == '.' || c == 'e' || c == 'E') { isFloat = true; Advance(); continue; }
            if ((c == '+' || c == '-') && (m_text[m_pos - 1] == 'e' || m_text[m_pos - 1] == 'E')) {
                Advance();
                continue;
            }
            break;
        }

        const std::string_view raw = m_text.substr(start, m_pos - start);
        if (raw.empty() || raw == "-" || raw == "+") {
            Error("doc.json.unexpected_char",
                  Fmt("값이 와야 할 자리에 '{}' 가 있다", std::string(1, Peek())),
                  "문자열은 겹따옴표로 감싸야 한다. JSON 에 홑따옴표는 없다");
            Advance();
            return Value{};
        }

        if (!isFloat) {
            i64 i = 0;
            if (ParseI64(raw, i)) return Value{i};
        }
        f64 f = 0.0;
        if (ParseF64(raw, f)) return Value{f};

        Error("doc.json.bad_number", Fmt("숫자로 읽을 수 없다: {}", raw));
        return Value{};
    }

    std::string ParseString() {
        const Mark open = Here();
        Advance();   // 여는 따옴표

        std::string out;
        while (!AtEnd()) {
            const char c = Peek();
            if (c == '"') { Advance(); return out; }
            if (c == '\n') break;   // JSON 문자열은 줄을 넘지 못한다

            if (c != '\\') { out += c; Advance(); continue; }

            Advance();   // 역슬래시
            if (AtEnd()) break;
            const char e = Peek();
            Advance();
            switch (e) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u': {
                    u32 code = 0;
                    bool ok = true;
                    for (int k = 0; k < 4; ++k) {
                        const char h = Peek();
                        const int digit =
                            (h >= '0' && h <= '9') ? h - '0' :
                            (h >= 'a' && h <= 'f') ? h - 'a' + 10 :
                            (h >= 'A' && h <= 'F') ? h - 'A' + 10 : -1;
                        if (digit < 0) { ok = false; break; }
                        code = code * 16 + static_cast<u32>(digit);
                        Advance();
                    }
                    if (!ok) {
                        Error("doc.json.bad_escape", "\\u 뒤에 16진수 4자리가 와야 한다");
                    } else {
                        AppendUtf8(out, code);
                    }
                    break;
                }
                default:
                    Error("doc.json.bad_escape",
                          Fmt("알 수 없는 이스케이프: \\{}", std::string(1, e)));
                    out += e;
                    break;
            }
        }

        Diagnostic& d = m_diags.Error("doc.json.unterminated_string", "문자열이 닫히지 않았다");
        d.hint = "겹따옴표를 닫아라. 줄바꿈을 넣으려면 \\n 을 쓴다";
        d.mark = open;
        return out;
    }

    Value ParseObject(u32 depth) {
        const Mark open = Here();
        Advance();   // '{'
        Value obj = Value::MakeMap();

        while (true) {
            SkipTrivia();
            if (AtEnd()) {
                Diagnostic& d = m_diags.Error("doc.json.unterminated_object", "'{' 가 닫히지 않았다");
                d.mark = open;
                return obj;
            }
            if (Peek() == '}') { Advance(); break; }

            if (Peek() != '"') {
                Error("doc.json.expected_key",
                      Fmt("객체의 키가 와야 한다. '{}' 를 만났다", std::string(1, Peek())),
                      "JSON 의 키는 반드시 겹따옴표 문자열이다");
                // 복구: 다음 쉼표나 닫는 괄호까지 건너뛴다
                while (!AtEnd() && Peek() != ',' && Peek() != '}') Advance();
                if (!AtEnd() && Peek() == ',') { Advance(); continue; }
                continue;
            }

            const Mark  keyMark = Here();
            std::string key     = ParseString();

            SkipTrivia();
            if (Peek() != ':') {
                Diagnostic& d = m_diags.Error("doc.json.expected_colon",
                                              Fmt("키 '{}' 뒤에 ':' 가 없다", key));
                d.mark = Here();
                while (!AtEnd() && Peek() != ',' && Peek() != '}') Advance();
            } else {
                Advance();
                Value v = ParseValue(depth + 1);
                if (obj.Has(key) && m_opts.duplicateKeyIsError) {
                    Diagnostic& d = m_diags.Error("doc.json.duplicate_key",
                                                  Fmt("키 '{}' 가 이미 있다", key));
                    d.mark = keyMark;
                } else {
                    obj.Set(std::move(key), std::move(v));
                }
            }

            SkipTrivia();
            if (Peek() == ',') {
                Advance();
                SkipTrivia();
                if (Peek() == '}') {
                    if (!m_opts.allowTrailingComma) {
                        Error("doc.json.trailing_comma", "마지막 항목 뒤에 쉼표가 있다");
                    }
                    Advance();
                    break;
                }
                continue;
            }
            if (Peek() == '}') { Advance(); break; }
            if (AtEnd()) continue;

            Error("doc.json.expected_separator",
                  Fmt("',' 또는 '}}' 가 와야 한다. '{}' 를 만났다", std::string(1, Peek())));
            Advance();
        }
        return obj;
    }

    Value ParseArray(u32 depth) {
        const Mark open = Here();
        Advance();   // '['
        Value arr = Value::MakeSeq();

        while (true) {
            SkipTrivia();
            if (AtEnd()) {
                Diagnostic& d = m_diags.Error("doc.json.unterminated_array", "'[' 가 닫히지 않았다");
                d.mark = open;
                return arr;
            }
            if (Peek() == ']') { Advance(); break; }

            arr.Push(ParseValue(depth + 1));

            SkipTrivia();
            if (Peek() == ',') {
                Advance();
                SkipTrivia();
                if (Peek() == ']') {
                    if (!m_opts.allowTrailingComma) {
                        Error("doc.json.trailing_comma", "마지막 항목 뒤에 쉼표가 있다");
                    }
                    Advance();
                    break;
                }
                continue;
            }
            if (Peek() == ']') { Advance(); break; }
            if (AtEnd()) continue;

            Error("doc.json.expected_separator",
                  Fmt("',' 또는 ']' 가 와야 한다. '{}' 를 만났다", std::string(1, Peek())));
            Advance();
        }
        return arr;
    }

    static void AppendUtf8(std::string& out, u32 code) {
        if (code < 0x80) {
            out += static_cast<char>(code);
        } else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }

    std::string_view        m_text;
    DiagnosticBag&          m_diags;
    const JsonParseOptions& m_opts;
    usize                   m_pos       = 0;
    usize                   m_lineStart = 0;
    u32                     m_line      = 1;
};

} // namespace

bool ParseJson(std::string_view text,
               Value& outRoot,
               DiagnosticBag& diagnostics,
               const JsonParseOptions& options) {
    JsonParser parser(text, diagnostics, options);
    const bool ok = parser.Parse(outRoot);
    diagnostics.AttachSnippets(std::string(text));
    return ok;
}

} // namespace alice::doc
