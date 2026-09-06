// SPDX-License-Identifier: MIT
#include "YamlParser.h"

#include "Foundation/StringUtil.h"

#include <algorithm>

namespace alice::doc {
namespace {

constexpr usize kNpos = std::string_view::npos;

struct Line {
    u32         number        = 0;   ///< 1-기반
    u32         indent        = 0;   ///< 앞쪽 공백 개수
    u32         contentColumn = 1;   ///< content 첫 글자의 1-기반 열
    u32         offset        = 0;   ///< 파일 시작 기준 줄 시작 바이트 오프셋
    std::string content;             ///< 주석 제거 + 우측 트림
    std::string comment;             ///< 잘라낸 주석 본문 (힌트용)
    std::string raw;                 ///< 원문 그대로 (블록 스칼라가 쓴다)
    bool        blank = true;
};

bool IsSeqEntry(std::string_view c) noexcept {
    if (c.empty() || c[0] != '-') return false;
    return c.size() == 1 || c[1] == ' ';
}

/// 'key: value' 의 콜론 위치. 따옴표와 플로우 괄호 안은 건너뛴다.
/// 콜론은 뒤에 공백이 오거나 줄 끝이어야 한다 — 그래야 "http://x" 가 키로 오해되지 않는다.
usize FindKeyColon(std::string_view s) noexcept {
    bool inSingle = false;
    bool inDouble = false;
    int  depth    = 0;

    for (usize i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (inDouble) {
            if (c == '\\') { ++i; continue; }
            if (c == '"')  inDouble = false;
            continue;
        }
        if (inSingle) {
            if (c == '\'') {
                if (i + 1 < s.size() && s[i + 1] == '\'') { ++i; continue; }  // '' 이스케이프
                inSingle = false;
            }
            continue;
        }
        switch (c) {
            case '"':  inDouble = true; break;
            case '\'': inSingle = true; break;
            case '[': case '{': ++depth; break;
            case ']': case '}': --depth; break;
            case ':':
                if (depth == 0 && (i + 1 == s.size() || s[i + 1] == ' ')) return i;
                break;
            default: break;
        }
    }
    return kNpos;
}

/// 따옴표 밖의 '#' 를 주석 시작으로 보고 잘라낸다.
/// YAML 규칙대로 '#' 앞에 공백이 있어야 주석이다(줄 맨 앞은 예외적으로 항상 주석).
void SplitComment(std::string_view content, std::string& outBody, std::string& outComment) {
    bool inSingle = false;
    bool inDouble = false;

    for (usize i = 0; i < content.size(); ++i) {
        const char c = content[i];
        if (inDouble) {
            if (c == '\\') { ++i; continue; }
            if (c == '"')  inDouble = false;
            continue;
        }
        if (inSingle) {
            if (c == '\'') {
                if (i + 1 < content.size() && content[i + 1] == '\'') { ++i; continue; }
                inSingle = false;
            }
            continue;
        }
        if (c == '"')  { inDouble = true; continue; }
        if (c == '\'') { inSingle = true; continue; }

        if (c == '#' && (i == 0 || content[i - 1] == ' ' || content[i - 1] == '\t')) {
            outBody    = std::string(TrimRight(content.substr(0, i)));
            outComment = std::string(Trim(content.substr(i + 1)));
            return;
        }
    }
    outBody = std::string(TrimRight(content));
    outComment.clear();
}

bool LooksLikeHexColor(std::string_view s) noexcept {
    if (s.size() != 3 && s.size() != 6 && s.size() != 8) return false;
    for (const char c : s) {
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return true;
}

/// 평문 스칼라의 타입을 판정한다.
///
/// YAML 1.1 의 yes/no/on/off 는 **일부러 불리언으로 보지 않는다.**
/// 노르웨이 국가코드 NO 가 false 가 되는 유명한 사고를 여기서 원천 차단한다.
/// 불리언은 true / false 뿐이다. 이 규칙은 Wiki 의 문서 문법 페이지에 명시한다.
Value ResolveScalar(std::string_view text) {
    if (text.empty()) return Value{};
    if (text == "null" || text == "Null" || text == "NULL" || text == "~") return Value{};
    if (text == "true"  || text == "True"  || text == "TRUE")  return Value{true};
    if (text == "false" || text == "False" || text == "FALSE") return Value{false};

    i64 asInt = 0;
    if (ParseI64(text, asInt)) return Value{asInt};

    f64 asFloat = 0.0;
    if (ParseF64(text, asFloat)) {
        // ParseF64 는 "1e5" 같은 것도 받는다. 정수로 읽히지 않았으니 실수로 확정한다.
        return Value{asFloat};
    }
    return Value{std::string(text)};
}

} // namespace

// ── 파서 본체 ──────────────────────────────────────────────────────────────
namespace {

class YamlParser {
public:
    YamlParser(std::string_view text, DiagnosticBag& diags, const YamlParseOptions& opts)
        : m_text(text), m_diags(diags), m_opts(opts) {}

    bool Parse(Value& outRoot) {
        SplitLines();
        SkipLeadingDirectives();

        outRoot = ParseNode(-1, 0);

        // 남은 줄이 있으면 구조가 어긋난 것이다. 조용히 버리지 않는다.
        SkipBlank();
        if (!AtEnd()) {
            const Line& l = Cur();
            if (Trim(l.content) == "---") {
                Error(l, "doc.parse.multi_document",
                      "문서 하나에 YAML 문서가 여러 개 있다",
                      "이 엔진은 파일 하나에 문서 하나만 쓴다. 나머지를 별도 파일로 분리하라");
            } else {
                Error(l, "doc.parse.trailing_content",
                      Fmt("구조 밖에 남은 내용이 있다: {}", l.content),
                      "들여쓰기가 어긋났을 가능성이 높다. 위 블록과 열을 맞춰라");
            }
        }
        return !m_diags.HasErrors();
    }

private:
    // ── 줄 분할 ────────────────────────────────────────────────────────────
    void SplitLines() {
        u32 number = 1;
        usize start = 0;

        while (start <= m_text.size()) {
            usize end = m_text.find('\n', start);
            if (end == kNpos) end = m_text.size();
            const std::string_view raw = m_text.substr(start, end - start);

            Line line;
            line.number = number;
            line.offset = static_cast<u32>(start);
            line.raw    = std::string(raw);

            // 들여쓰기 계산. 탭은 여기서 잡는다 — 가장 흔한 YAML 사고다.
            usize i = 0;
            bool tabSeen = false;
            while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\t')) {
                if (raw[i] == '\t') tabSeen = true;
                ++i;
            }
            line.indent        = static_cast<u32>(i);
            line.contentColumn = static_cast<u32>(i) + 1;

            const std::string_view rest = raw.substr(i);
            SplitComment(rest, line.content, line.comment);
            line.blank = line.content.empty();

            if (tabSeen && !line.blank) {
                Diagnostic& d = m_diags.Error("doc.parse.tab_indent", "들여쓰기에 탭이 있다");
                d.hint   = "YAML 은 탭으로 들여쓸 수 없다. 공백 2칸을 쓰라";
                d.mark   = Mark{line.number, 1, line.offset};
            }

            m_lines.push_back(std::move(line));

            if (end == m_text.size()) break;
            start = end + 1;
            ++number;
        }
    }

    void SkipLeadingDirectives() {
        SkipBlank();
        while (!AtEnd()) {
            const std::string_view c = Trim(Cur().content);
            if (c == "---") { Advance(); SkipBlank(); continue; }
            if (StartsWith(c, "%")) {
                Error(Cur(), "doc.parse.directive_unsupported",
                      Fmt("YAML 지시자를 지원하지 않는다: {}", c),
                      "%YAML / %TAG 줄을 지워라. 이 엔진은 스키마로 버전을 관리한다");
                Advance();
                SkipBlank();
                continue;
            }
            break;
        }
    }

    // ── 커서 ───────────────────────────────────────────────────────────────
    bool  AtEnd() const noexcept { return m_index >= m_lines.size(); }
    Line& Cur() noexcept { return m_lines[m_index]; }
    void  Advance() noexcept { ++m_index; }
    void  SkipBlank() noexcept {
        while (m_index < m_lines.size() && m_lines[m_index].blank) ++m_index;
    }

    Mark MarkOf(const Line& l) const noexcept {
        return Mark{l.number, l.contentColumn, l.offset + l.indent};
    }
    Mark MarkAt(const Line& l, u32 column) const noexcept {
        return Mark{l.number, column, l.offset + column - 1};
    }

    Diagnostic& Error(const Line& l, std::string code, std::string message, std::string hint = {}) {
        Diagnostic& d = m_diags.Error(std::move(code), std::move(message));
        d.hint = std::move(hint);
        d.mark = MarkOf(l);
        return d;
    }
    Diagnostic& ErrorAt(const Line& l, u32 column, std::string code, std::string message,
                        std::string hint = {}) {
        Diagnostic& d = m_diags.Error(std::move(code), std::move(message));
        d.hint = std::move(hint);
        d.mark = MarkAt(l, column);
        return d;
    }

    // ── 노드 ───────────────────────────────────────────────────────────────
    Value ParseNode(i32 parentIndent, u32 depth) {
        if (depth > m_opts.maxDepth) {
            if (!AtEnd()) {
                Error(Cur(), "doc.parse.too_deep",
                      Fmt("중첩이 너무 깊다 (한계 {})", m_opts.maxDepth),
                      "문서를 나눠라. 이 정도 깊이는 사람도 AI 도 읽지 못한다");
            }
            return Value{};
        }

        SkipBlank();
        if (AtEnd()) return Value{};

        const Line& line = Cur();
        if (static_cast<i32>(line.indent) <= parentIndent) return Value{};

        const i32 indent = static_cast<i32>(line.indent);
        if (IsSeqEntry(line.content))            return ParseBlockSeq(indent, depth);
        if (FindKeyColon(line.content) != kNpos) return ParseBlockMap(indent, depth);

        // 콜론이 없다. 진짜 단독 스칼라일 수도 있고, 콜론을 빠뜨린 키일 수도 있다.
        // 같은 들여쓰기에 형제 줄이 더 있으면 후자다. 그때는 맵으로 넘겨서
        // "키: 값 형태가 아니다" 라는 정확한 진단이 나오게 한다.
        // (여기서 스칼라로 삼켜버리면 남은 줄이 전부 trailing_content 로 뭉개진다.)
        if (BlockContinuesAt(indent)) return ParseBlockMap(indent, depth);

        Value v = ParseFlowOrScalar(line.content, line, line.contentColumn, depth);
        Advance();
        return v;
    }

    /// m_index 다음에 같은 들여쓰기의 형제 줄이 더 있는가.
    bool BlockContinuesAt(i32 indent) const noexcept {
        for (usize i = m_index + 1; i < m_lines.size(); ++i) {
            const Line& l = m_lines[i];
            if (l.blank) continue;
            if (static_cast<i32>(l.indent) < indent) return false;
            if (static_cast<i32>(l.indent) == indent) return true;
        }
        return false;
    }

    Value ParseBlockSeq(i32 indent, u32 depth) {
        Value seq = Value::MakeSeq();
        seq.mark = MarkOf(Cur());

        while (true) {
            SkipBlank();
            if (AtEnd()) break;
            if (static_cast<i32>(Cur().indent) < indent) break;

            if (static_cast<i32>(Cur().indent) > indent) {
                Error(Cur(), "doc.parse.unexpected_indent",
                      "들여쓰기가 시퀀스 항목보다 깊다",
                      Fmt("이 줄을 {}칸 들여쓰기에 맞추거나, 위 항목의 값으로 만들려면 '- ' 뒤에 두라", indent));
                Advance();
                continue;
            }
            if (!IsSeqEntry(Cur().content)) break;   // 같은 들여쓰기의 맵 키 → 시퀀스 끝

            const usize lineIdx = m_index;
            Line& line = m_lines[lineIdx];

            // 대시 뒤 첫 글자 위치를 찾는다.
            usize p = 1;
            while (p < line.content.size() && line.content[p] == ' ') ++p;

            if (p >= line.content.size()) {
                // "-" 만 있는 줄 → 값은 다음 블록에 있다.
                Advance();
                seq.Push(ParseNode(indent, depth + 1));
                continue;
            }

            // "- key: v" 를 "key: v" 한 줄로 재작성한다. 열 정보는 그대로 유지한다.
            const std::string rest   = line.content.substr(p);
            const u32         newCol = line.contentColumn + static_cast<u32>(p);
            const i32         newInd = indent + static_cast<i32>(p);

            line.content       = rest;
            line.indent        = static_cast<u32>(newInd);
            line.contentColumn = newCol;

            if (IsSeqEntry(rest)) {
                seq.Push(ParseBlockSeq(newInd, depth + 1));
            } else if (FindKeyColon(rest) != kNpos) {
                seq.Push(ParseBlockMap(newInd, depth + 1));
            } else {
                Value item = ParseFlowOrScalar(rest, m_lines[lineIdx], newCol, depth + 1);
                Advance();
                seq.Push(std::move(item));
            }
        }
        return seq;
    }

    Value ParseBlockMap(i32 indent, u32 depth) {
        Value map = Value::MakeMap();
        map.mark = MarkOf(Cur());

        while (true) {
            SkipBlank();
            if (AtEnd()) break;
            if (static_cast<i32>(Cur().indent) < indent) break;

            if (static_cast<i32>(Cur().indent) > indent) {
                Error(Cur(), "doc.parse.unexpected_indent",
                      "들여쓰기가 형제 키보다 깊다",
                      Fmt("위 키의 값으로 만들려면 그 키를 'key:' 로 끝내라. 아니면 {}칸에 맞춰라", indent));
                Advance();
                continue;
            }
            if (IsSeqEntry(Cur().content)) break;   // 같은 들여쓰기의 시퀀스 → 맵 끝

            const usize lineIdx = m_index;
            const Line& line = m_lines[lineIdx];

            if (StartsWith(line.content, "? ")) {
                Error(line, "doc.parse.complex_key_unsupported",
                      "복합 키(? ...)를 지원하지 않는다",
                      "키는 항상 문자열이어야 한다");
                Advance();
                continue;
            }

            const usize colon = FindKeyColon(line.content);
            if (colon == kNpos) {
                Error(line, "doc.parse.expected_key",
                      Fmt("'키: 값' 형태가 아니다: {}", line.content),
                      "콜론 뒤에는 공백이 있어야 한다. 값에 콜론이 들어가면 따옴표로 감싸라");
                Advance();
                continue;
            }

            const u32   keyColumn = line.contentColumn;
            std::string key       = ParseKeyText(line, std::string_view(line.content).substr(0, colon));

            usize vp = colon + 1;
            while (vp < line.content.size() && line.content[vp] == ' ') ++vp;
            const std::string rest    = (vp < line.content.size()) ? line.content.substr(vp) : std::string{};
            const u32         restCol = line.contentColumn + static_cast<u32>(vp);

            Value value;
            if (!rest.empty() && (rest[0] == '|' || rest[0] == '>')) {
                const u32 headerLine = line.number;
                value = ParseBlockScalar(rest, indent, headerLine);
            } else if (rest.empty()) {
                // 값이 다음 블록에 있거나, 아무것도 없거나(null), 주석에 먹혔거나.
                const std::string comment    = line.comment;
                const u32         commentLn  = line.number;
                const u32         commentCol = keyColumn;
                Advance();
                value = ParseNode(indent, depth + 1);

                if (value.IsNull() && !comment.empty()) {
                    // 이 엔진에서 압도적으로 흔한 실수: color: #ff8800
                    // YAML 은 이걸 주석으로 먹어버린다. 조용히 null 로 두면 콘텐츠 버그가 된다.
                    if (LooksLikeHexColor(comment)) {
                        Diagnostic& d = m_diags.Error(
                            "doc.parse.value_eaten_by_comment",
                            Fmt("'{}' 의 값이 주석으로 해석되어 사라졌다", key));
                        d.hint = Fmt("YAML 에서 공백 뒤의 '#' 는 주석이다. 따옴표로 감싸라: {}: \"#{}\"",
                                        key, comment);
                        d.mark = Mark{commentLn, commentCol, 0};
                    }
                }
            } else {
                value = ParseFlowOrScalar(rest, m_lines[lineIdx], restCol, depth + 1);
                Advance();
            }

            if (map.Has(key)) {
                if (m_opts.duplicateKeyIsError) {
                    ErrorAt(m_lines[lineIdx], keyColumn, "doc.parse.duplicate_key",
                            Fmt("키 '{}' 가 이 맵에 이미 있다", key),
                            "덮어쓰기를 의도했다면 위쪽 정의를 지워라. 목록이 필요하면 시퀀스를 쓰라");
                } else {
                    map.Set(key, std::move(value));
                }
                continue;
            }
            map.Set(std::move(key), std::move(value));
        }
        return map;
    }

    std::string ParseKeyText(const Line& line, std::string_view raw) {
        const std::string_view k = Trim(raw);
        if (k.size() >= 2 && ((k.front() == '"' && k.back() == '"') ||
                              (k.front() == '\'' && k.back() == '\''))) {
            DiagnosticBag scratch;
            return UnquoteScalar(k, line, line.contentColumn, scratch).AsString();
        }
        if (k.empty()) {
            Error(line, "doc.parse.empty_key", "키가 비어 있다",
                  "키 이름을 적거나 이 줄을 지워라");
        }
        return std::string(k);
    }

    // ── 블록 스칼라 ────────────────────────────────────────────────────────
    Value ParseBlockScalar(const std::string& header, i32 parentIndent, u32 headerLine) {
        const char style = header[0];                     // '|' 또는 '>'
        char chomp = ' ';                                  // ' '=clip, '-'=strip, '+'=keep
        for (usize i = 1; i < header.size(); ++i) {
            if (header[i] == '-' || header[i] == '+') { chomp = header[i]; break; }
            if (header[i] == ' ') continue;
            // 숫자 지시자(|2)는 지원하지 않는다 — 쓸 일이 없고 헷갈리기만 한다.
            const Line& l = m_lines[m_index];
            Error(l, "doc.parse.block_scalar_indicator",
                  Fmt("블록 스칼라 지시자를 지원하지 않는다: {}", header),
                  "'|', '|-', '|+', '>', '>-', '>+' 만 쓸 수 있다");
            break;
        }

        Advance();   // 헤더 줄 소비

        // 본문 들여쓰기는 첫 비어있지 않은 줄이 정한다.
        i32 contentIndent = -1;
        {
            usize probe = m_index;
            while (probe < m_lines.size()) {
                const Line& l = m_lines[probe];
                if (!l.raw.empty() && Trim(l.raw).empty()) { ++probe; continue; }  // 공백만
                if (l.raw.empty()) { ++probe; continue; }
                if (static_cast<i32>(l.indent) <= parentIndent) break;
                contentIndent = static_cast<i32>(l.indent);
                break;
            }
        }

        std::vector<std::string> body;
        if (contentIndent >= 0) {
            while (m_index < m_lines.size()) {
                const Line& l = m_lines[m_index];
                const bool empty = Trim(l.raw).empty();
                if (!empty && static_cast<i32>(l.indent) < contentIndent) break;

                if (empty) {
                    body.emplace_back();
                } else {
                    body.push_back(l.raw.substr(static_cast<usize>(contentIndent)));
                }
                Advance();
            }
        }

        // 뒤쪽 빈 줄은 청킹 규칙에 따라 나중에 다시 붙인다.
        usize trailingBlanks = 0;
        while (!body.empty() && body.back().empty()) { body.pop_back(); ++trailingBlanks; }

        std::string text;
        if (style == '|') {
            for (usize i = 0; i < body.size(); ++i) {
                if (i) text += '\n';
                text += body[i];
            }
        } else {
            // folded: 이어지는 줄은 공백으로 접고, 빈 줄은 줄바꿈으로 남긴다.
            for (usize i = 0; i < body.size(); ++i) {
                if (i > 0) text += body[i].empty() ? '\n' : ' ';
                text += body[i];
            }
        }

        if (chomp == '+') {
            if (!body.empty()) text += '\n';
            for (usize i = 0; i < trailingBlanks; ++i) text += '\n';
        } else if (chomp == ' ') {
            if (!body.empty()) text += '\n';          // clip: 줄바꿈 하나만 남긴다
        }
        // chomp == '-' : 아무것도 붙이지 않는다

        Value v{std::move(text)};
        v.mark = Mark{headerLine, 1, 0};
        return v;
    }

    // ── 스칼라 / 플로우 ────────────────────────────────────────────────────
    Value ParseFlowOrScalar(const std::string& text, const Line& line, u32 column, u32 depth) {
        const std::string_view t = Trim(text);
        if (t.empty()) return Value{};

        switch (t.front()) {
            case '[': case '{': {
                usize cursor = 0;
                Value v = ParseFlow(t, cursor, line, column, depth);
                const std::string_view tail = Trim(t.substr(cursor));
                if (!tail.empty()) {
                    ErrorAt(line, column + static_cast<u32>(cursor), "doc.parse.flow_trailing",
                            Fmt("플로우 컬렉션 뒤에 남은 내용이 있다: {}", tail),
                            "닫는 괄호 위치를 확인하라");
                }
                return v;
            }
            case '"': case '\'': {
                // 닫는 따옴표까지만 값이다. 뒤에 뭐가 더 붙어 있으면 그건 실수다.
                const usize            end    = ScanQuoted(t, 0);
                const std::string_view quoted = t.substr(0, end);
                const std::string_view tail   = Trim(t.substr(end));
                Value v = UnquoteScalar(quoted, line, column, m_diags);
                if (!tail.empty()) {
                    ErrorAt(line, column + static_cast<u32>(end), "doc.parse.scalar_trailing",
                            Fmt("따옴표 문자열 뒤에 남은 내용이 있다: {}", tail),
                            "값 전체를 따옴표 안에 넣거나, 남은 부분을 지워라");
                }
                return v;
            }
            case '&':
                ErrorAt(line, column, "doc.parse.anchor_unsupported",
                        "앵커(&)를 지원하지 않는다",
                        "값을 재사용하려면 별도 파일로 빼고 $ref 로 가리켜라");
                return Value{};
            case '*':
                ErrorAt(line, column, "doc.parse.alias_unsupported",
                        "별칭(*)을 지원하지 않는다",
                        "값을 재사용하려면 $ref 를 쓰라");
                return Value{};
            case '!':
                ErrorAt(line, column, "doc.parse.tag_unsupported",
                        "태그(!)를 지원하지 않는다",
                        "타입은 스키마가 정한다. 태그를 지워라");
                return Value{};
            default:
                break;
        }

        Value v = ResolveScalar(t);
        v.mark = Mark{line.number, column, line.offset + column - 1};
        return v;
    }

    Value ParseFlow(std::string_view s, usize& i, const Line& line, u32 baseColumn, u32 depth) {
        if (depth > m_opts.maxDepth) {
            ErrorAt(line, baseColumn, "doc.parse.too_deep", "플로우 중첩이 너무 깊다");
            return Value{};
        }

        SkipFlowSpace(s, i);
        if (i >= s.size()) return Value{};

        if (s[i] == '[') return ParseFlowSeq(s, i, line, baseColumn, depth);
        if (s[i] == '{') return ParseFlowMap(s, i, line, baseColumn, depth);

        // 플로우 안의 스칼라: 구분자 전까지.
        const usize start = i;
        if (s[i] == '"' || s[i] == '\'') {
            const usize end = ScanQuoted(s, i);
            const std::string_view raw = s.substr(start, end - start);
            i = end;
            return UnquoteScalar(raw, line, baseColumn + static_cast<u32>(start), m_diags);
        }
        while (i < s.size() && s[i] != ',' && s[i] != ']' && s[i] != '}') ++i;
        const std::string_view raw = Trim(s.substr(start, i - start));
        Value v = ResolveScalar(raw);
        v.mark = Mark{line.number, baseColumn + static_cast<u32>(start), 0};
        return v;
    }

    Value ParseFlowSeq(std::string_view s, usize& i, const Line& line, u32 baseColumn, u32 depth) {
        const u32 openColumn = baseColumn + static_cast<u32>(i);
        ++i;   // '['
        Value seq = Value::MakeSeq();
        seq.mark = Mark{line.number, openColumn, 0};

        while (true) {
            SkipFlowSpace(s, i);
            if (i >= s.size()) {
                ErrorAt(line, openColumn, "doc.parse.flow_unterminated",
                        "'[' 가 같은 줄에서 닫히지 않았다",
                        "플로우 컬렉션은 한 줄 안에서 끝나야 한다. 길면 블록 시퀀스('- ')로 쓰라");
                return seq;
            }
            if (s[i] == ']') { ++i; break; }

            seq.Push(ParseFlow(s, i, line, baseColumn, depth + 1));

            SkipFlowSpace(s, i);
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            if (i < s.size() && s[i] == ']') { ++i; break; }
            if (i >= s.size()) continue;   // 다음 루프에서 미종료 에러

            ErrorAt(line, baseColumn + static_cast<u32>(i), "doc.parse.flow_expected_separator",
                    Fmt("',' 또는 ']' 가 와야 한다: '{}'", std::string(1, s[i])),
                    "항목 사이에는 쉼표가 필요하다");
            ++i;
        }
        return seq;
    }

    Value ParseFlowMap(std::string_view s, usize& i, const Line& line, u32 baseColumn, u32 depth) {
        const u32 openColumn = baseColumn + static_cast<u32>(i);
        ++i;   // '{'
        Value map = Value::MakeMap();
        map.mark = Mark{line.number, openColumn, 0};

        while (true) {
            SkipFlowSpace(s, i);
            if (i >= s.size()) {
                ErrorAt(line, openColumn, "doc.parse.flow_unterminated",
                        "'{' 가 같은 줄에서 닫히지 않았다",
                        "플로우 맵은 한 줄 안에서 끝나야 한다");
                return map;
            }
            if (s[i] == '}') { ++i; break; }

            // 키
            const usize keyStart = i;
            std::string key;
            if (s[i] == '"' || s[i] == '\'') {
                const usize end = ScanQuoted(s, i);
                key = UnquoteScalar(s.substr(keyStart, end - keyStart), line,
                                    baseColumn + static_cast<u32>(keyStart), m_diags).AsString();
                i = end;
            } else {
                while (i < s.size() && s[i] != ':' && s[i] != ',' && s[i] != '}') ++i;
                key = std::string(Trim(s.substr(keyStart, i - keyStart)));
            }

            SkipFlowSpace(s, i);
            if (i >= s.size() || s[i] != ':') {
                ErrorAt(line, baseColumn + static_cast<u32>(keyStart), "doc.parse.flow_expected_colon",
                        Fmt("플로우 맵의 키 '{}' 뒤에 ':' 가 없다", key));
                // 복구: 다음 쉼표나 닫는 괄호까지 건너뛴다
                while (i < s.size() && s[i] != ',' && s[i] != '}') ++i;
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size()) { ++i; }
                break;
            }
            ++i;   // ':'

            Value value = ParseFlow(s, i, line, baseColumn, depth + 1);
            if (map.Has(key) && m_opts.duplicateKeyIsError) {
                ErrorAt(line, baseColumn + static_cast<u32>(keyStart), "doc.parse.duplicate_key",
                        Fmt("키 '{}' 가 이 플로우 맵에 이미 있다", key));
            } else {
                map.Set(std::move(key), std::move(value));
            }

            SkipFlowSpace(s, i);
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            if (i < s.size() && s[i] == '}') { ++i; break; }
        }
        return map;
    }

    static void SkipFlowSpace(std::string_view s, usize& i) noexcept {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    }

    /// 따옴표 스칼라의 끝(닫는 따옴표 다음) 인덱스를 돌려준다.
    static usize ScanQuoted(std::string_view s, usize start) noexcept {
        const char quote = s[start];
        usize i = start + 1;
        while (i < s.size()) {
            if (quote == '"' && s[i] == '\\') { i += 2; continue; }
            if (s[i] == quote) {
                if (quote == '\'' && i + 1 < s.size() && s[i + 1] == '\'') { i += 2; continue; }
                return i + 1;
            }
            ++i;
        }
        return s.size();
    }

    Value UnquoteScalar(std::string_view raw, const Line& line, u32 column, DiagnosticBag& diags) {
        if (raw.size() < 2 || (raw.front() != '"' && raw.front() != '\'')) {
            return Value{std::string(raw)};
        }
        const char quote = raw.front();
        if (raw.back() != quote || raw.size() < 2) {
            Diagnostic& d = diags.Error("doc.parse.unterminated_string", "문자열의 따옴표가 닫히지 않았다");
            d.hint = "같은 종류의 따옴표로 닫아라";
            d.mark = Mark{line.number, column, 0};
            return Value{std::string(raw.substr(1))};
        }

        const std::string_view body = raw.substr(1, raw.size() - 2);
        std::string out;
        out.reserve(body.size());

        if (quote == '\'') {
            // 홑따옴표: '' 만 이스케이프다. 나머지는 전부 리터럴.
            for (usize i = 0; i < body.size(); ++i) {
                if (body[i] == '\'' && i + 1 < body.size() && body[i + 1] == '\'') { out += '\''; ++i; }
                else out += body[i];
            }
            return Value{std::move(out)};
        }

        for (usize i = 0; i < body.size(); ++i) {
            if (body[i] != '\\') { out += body[i]; continue; }
            if (++i >= body.size()) break;
            switch (body[i]) {
                case 'n':  out += '\n'; break;
                case 't':  out += '\t'; break;
                case 'r':  out += '\r'; break;
                case '0':  out += '\0'; break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'u': {
                    if (i + 4 >= body.size()) {
                        Diagnostic& d = diags.Error("doc.parse.bad_escape",
                                                    "\\u 뒤에 16진수 4자리가 없다");
                        d.mark = Mark{line.number, column, 0};
                        i = body.size();
                        break;
                    }
                    i64 code = 0;
                    if (!ParseI64(body.substr(i + 1, 4), code)) {
                        // 16진수로 다시 시도
                        code = 0;
                        bool ok = true;
                        for (usize k = 1; k <= 4; ++k) {
                            const char c = body[i + k];
                            const int digit =
                                (c >= '0' && c <= '9') ? c - '0' :
                                (c >= 'a' && c <= 'f') ? c - 'a' + 10 :
                                (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
                            if (digit < 0) { ok = false; break; }
                            code = code * 16 + digit;
                        }
                        if (!ok) {
                            Diagnostic& d = diags.Error("doc.parse.bad_escape",
                                                        "\\u 이스케이프가 16진수가 아니다");
                            d.mark = Mark{line.number, column, 0};
                        }
                    }
                    AppendUtf8(out, static_cast<u32>(code));
                    i += 4;
                    break;
                }
                default: {
                    Diagnostic& d = diags.Error(
                        "doc.parse.bad_escape",
                        Fmt("알 수 없는 이스케이프: \\{}", std::string(1, body[i])));
                    d.hint = "역슬래시 자체를 쓰려면 \\\\ 로 적어라";
                    d.mark = Mark{line.number, column, 0};
                    out += body[i];
                    break;
                }
            }
        }
        Value v{std::move(out)};
        v.mark = Mark{line.number, column, 0};
        return v;
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
    const YamlParseOptions& m_opts;
    std::vector<Line>       m_lines;
    usize                   m_index = 0;
};

} // namespace

bool ParseYaml(std::string_view text,
               Value& outRoot,
               DiagnosticBag& diagnostics,
               const YamlParseOptions& options) {
    YamlParser parser(text, diagnostics, options);
    const bool ok = parser.Parse(outRoot);
    diagnostics.AttachSnippets(std::string(text));
    return ok;
}

} // namespace alice::doc
