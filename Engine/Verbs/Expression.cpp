// SPDX-License-Identifier: MIT
#include "Expression.h"

#include "Foundation/StringUtil.h"

#include <algorithm>

namespace alice::verbs {
namespace {

// ── 토큰 ───────────────────────────────────────────────────────────────────
enum class Tok : u8 {
    End, Number, String, Identifier, Operator, LParen, RParen, Comma, Dot,
};

struct Token {
    Tok         kind = Tok::End;
    std::string text;
    f64         number = 0.0;
    bool        isInteger = false;
    u32         column = 0;   ///< 식 안에서의 1-기반 열
};

bool IsIdentStart(char c) noexcept {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
bool IsIdentChar(char c) noexcept {
    return IsIdentStart(c) || (c >= '0' && c <= '9');
}
bool IsDigit(char c) noexcept { return c >= '0' && c <= '9'; }

class Lexer {
public:
    Lexer(std::string_view text, DiagnosticBag& diags, Mark base)
        : m_text(text), m_diags(diags), m_base(base) {}

    std::vector<Token> Run() {
        std::vector<Token> tokens;
        while (true) {
            SkipSpace();
            if (m_pos >= m_text.size()) break;

            Token t;
            t.column = static_cast<u32>(m_pos) + 1;
            const char c = m_text[m_pos];

            if (IsDigit(c) || (c == '.' && m_pos + 1 < m_text.size() && IsDigit(m_text[m_pos + 1]))) {
                t = ReadNumber();
            } else if (c == '"' || c == '\'') {
                t = ReadString();
            } else if (IsIdentStart(c)) {
                t = ReadIdentifier();
            } else if (c == '(') { t.kind = Tok::LParen; t.text = "("; ++m_pos; }
            else if (c == ')') { t.kind = Tok::RParen; t.text = ")"; ++m_pos; }
            else if (c == ',') { t.kind = Tok::Comma;  t.text = ","; ++m_pos; }
            else if (c == '.') { t.kind = Tok::Dot;    t.text = "."; ++m_pos; }
            else { t = ReadOperator(); }

            if (t.kind == Tok::End) break;   // 복구 불가
            tokens.push_back(std::move(t));
        }

        Token end;
        end.kind   = Tok::End;
        end.column = static_cast<u32>(m_text.size()) + 1;
        tokens.push_back(std::move(end));
        return tokens;
    }

private:
    void SkipSpace() {
        while (m_pos < m_text.size() &&
               (m_text[m_pos] == ' ' || m_text[m_pos] == '\t' || m_text[m_pos] == '\n')) {
            ++m_pos;
        }
    }

    Mark MarkAt(usize column) const {
        // 식은 문서의 한 줄 안에 들어 있다. 기준 열에 식 내부 오프셋을 더한다.
        Mark m = m_base;
        if (m.line == 0) m.line = 1;
        m.column = (m_base.column ? m_base.column : 1) + static_cast<u32>(column);
        return m;
    }

    Token ReadNumber() {
        const usize start = m_pos;
        bool isFloat = false;
        while (m_pos < m_text.size() &&
               (IsDigit(m_text[m_pos]) || m_text[m_pos] == '.' ||
                m_text[m_pos] == 'e' || m_text[m_pos] == 'E')) {
            if (m_text[m_pos] == '.' || m_text[m_pos] == 'e' || m_text[m_pos] == 'E') isFloat = true;
            ++m_pos;
        }
        const std::string_view raw = m_text.substr(start, m_pos - start);

        Token t;
        t.kind   = Tok::Number;
        t.text   = std::string(raw);
        t.column = static_cast<u32>(start) + 1;

        if (!isFloat) {
            i64 i = 0;
            if (ParseI64(raw, i)) {
                t.number    = static_cast<f64>(i);
                t.isInteger = true;
                return t;
            }
        }
        if (!ParseF64(raw, t.number)) {
            Diagnostic& d = m_diags.Error("expr.bad_number",
                                          Fmt("숫자로 읽을 수 없다: {}", raw));
            d.mark = MarkAt(start);
        }
        return t;
    }

    Token ReadString() {
        const char quote = m_text[m_pos];
        const usize start = m_pos;
        ++m_pos;

        std::string value;
        bool closed = false;
        while (m_pos < m_text.size()) {
            const char c = m_text[m_pos];
            if (c == '\\' && m_pos + 1 < m_text.size()) {
                ++m_pos;
                const char e = m_text[m_pos];
                value += (e == 'n') ? '\n' : (e == 't') ? '\t' : e;
                ++m_pos;
                continue;
            }
            if (c == quote) { ++m_pos; closed = true; break; }
            value += c;
            ++m_pos;
        }

        if (!closed) {
            Diagnostic& d = m_diags.Error("expr.unterminated_string", "문자열이 닫히지 않았다");
            d.hint = "같은 종류의 따옴표로 닫아라";
            d.mark = MarkAt(start);
        }

        Token t;
        t.kind   = Tok::String;
        t.text   = std::move(value);
        t.column = static_cast<u32>(start) + 1;
        return t;
    }

    Token ReadIdentifier() {
        const usize start = m_pos;
        while (m_pos < m_text.size() && IsIdentChar(m_text[m_pos])) ++m_pos;

        Token t;
        t.text   = std::string(m_text.substr(start, m_pos - start));
        t.column = static_cast<u32>(start) + 1;
        // and / or / not 은 예약된 연산자다. 키워드를 기호로 쓰지 않는 이유는
        // && || ! 가 YAML 안에서 따옴표를 요구하는 경우가 있어서다.
        t.kind = (t.text == "and" || t.text == "or" || t.text == "not")
                     ? Tok::Operator : Tok::Identifier;
        return t;
    }

    Token ReadOperator() {
        static const char* kTwoChar[] = {"==", "!=", "<=", ">=", "&&", "||"};
        const usize start = m_pos;

        Token t;
        t.column = static_cast<u32>(start) + 1;

        if (m_pos + 1 < m_text.size()) {
            const std::string_view two = m_text.substr(m_pos, 2);
            for (const char* op : kTwoChar) {
                if (two == op) {
                    t.kind = Tok::Operator;
                    // && 와 || 도 받아주되 정규 이름으로 접는다. 진단 문구가 하나로 통일된다.
                    t.text = (two == "&&") ? "and" : (two == "||") ? "or" : std::string(two);
                    m_pos += 2;
                    return t;
                }
            }
        }

        const char c = m_text[m_pos];
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '<' || c == '>') {
            t.kind = Tok::Operator;
            t.text = std::string(1, c);
            ++m_pos;
            return t;
        }
        if (c == '!') {
            t.kind = Tok::Operator;
            t.text = "not";
            ++m_pos;
            return t;
        }
        if (c == '=') {
            Diagnostic& d = m_diags.Error("expr.assignment",
                                          "식에서는 대입할 수 없다");
            d.hint = "비교하려면 '==' 를 쓰라. 값을 바꾸는 일은 동사(do:)가 한다";
            d.mark = MarkAt(start);
            ++m_pos;
            t.kind = Tok::Operator;
            t.text = "==";
            return t;
        }

        Diagnostic& d = m_diags.Error("expr.unexpected_char",
                                      Fmt("식에 쓸 수 없는 문자다: '{}'", std::string(1, c)));
        d.mark = MarkAt(start);
        ++m_pos;
        t.kind = Tok::End;   // 파서에게 중단을 알린다
        return t;
    }

    std::string_view m_text;
    DiagnosticBag&   m_diags;
    Mark             m_base;
    usize            m_pos = 0;
};

// ── 파서 ───────────────────────────────────────────────────────────────────
int BinaryPrecedence(const std::string& op) noexcept {
    if (op == "or")  return 1;
    if (op == "and") return 2;
    if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") return 3;
    if (op == "+" || op == "-") return 4;
    if (op == "*" || op == "/" || op == "%") return 5;
    return 0;
}

class Parser {
public:
    Parser(std::vector<Token> tokens, DiagnosticBag& diags, Mark base)
        : m_tokens(std::move(tokens)), m_diags(diags), m_base(base) {}

    ExprPtr Run() {
        if (m_tokens.size() <= 1) {
            Diagnostic& d = m_diags.Error("expr.empty", "식이 비어 있다");
            d.mark = m_base;
            return nullptr;
        }
        ExprPtr expr = ParseBinary(0);
        if (!expr) return nullptr;
        if (Cur().kind != Tok::End) {
            Diagnostic& d = m_diags.Error(
                "expr.trailing",
                Fmt("식 뒤에 남은 내용이 있다: '{}'", Cur().text));
            d.hint = "괄호가 맞는지, 연산자를 빠뜨리지 않았는지 확인하라";
            d.mark = MarkOf(Cur());
        }
        return expr;
    }

private:
    const Token& Cur() const { return m_tokens[m_index]; }
    void Advance() { if (m_index + 1 < m_tokens.size()) ++m_index; }

    Mark MarkOf(const Token& t) const {
        Mark m = m_base;
        if (m.line == 0) m.line = 1;
        m.column = (m_base.column ? m_base.column : 1) + t.column - 1;
        return m;
    }

    ExprPtr Make(ExprKind kind, const Token& t) {
        auto e = std::make_shared<Expr>();
        e->kind = kind;
        e->mark = MarkOf(t);
        return e;
    }

    ExprPtr ParseBinary(int minPrecedence) {
        ExprPtr left = ParseUnary();
        if (!left) return nullptr;

        while (Cur().kind == Tok::Operator) {
            const int precedence = BinaryPrecedence(Cur().text);
            if (precedence == 0 || precedence < minPrecedence) break;

            const Token opToken = Cur();
            Advance();

            ExprPtr right = ParseBinary(precedence + 1);
            if (!right) return nullptr;

            auto node = Make(ExprKind::Binary, opToken);
            node->op = opToken.text;
            node->children.push_back(std::move(left));
            node->children.push_back(std::move(right));
            left = std::move(node);
        }
        return left;
    }

    ExprPtr ParseUnary() {
        if (Cur().kind == Tok::Operator && (Cur().text == "not" || Cur().text == "-")) {
            const Token opToken = Cur();
            Advance();
            ExprPtr operand = ParseUnary();
            if (!operand) return nullptr;
            auto node = Make(ExprKind::Unary, opToken);
            node->op = opToken.text;
            node->children.push_back(std::move(operand));
            return node;
        }
        return ParsePostfix();
    }

    ExprPtr ParsePostfix() {
        ExprPtr expr = ParsePrimary();
        if (!expr) return nullptr;

        while (true) {
            if (Cur().kind == Tok::Dot) {
                const Token dot = Cur();
                Advance();
                if (Cur().kind != Tok::Identifier) {
                    Diagnostic& d = m_diags.Error("expr.expected_name", "'.' 뒤에 이름이 와야 한다");
                    d.mark = MarkOf(Cur());
                    return expr;
                }
                auto node = Make(ExprKind::Member, dot);
                node->name = Cur().text;
                node->children.push_back(std::move(expr));
                Advance();
                expr = std::move(node);
                continue;
            }

            if (Cur().kind == Tok::LParen) {
                const Token open = Cur();
                Advance();
                auto node = Make(ExprKind::Call, open);
                node->children.push_back(std::move(expr));

                if (Cur().kind != Tok::RParen) {
                    while (true) {
                        ExprPtr arg = ParseBinary(0);
                        if (!arg) return node;
                        node->children.push_back(std::move(arg));
                        if (Cur().kind == Tok::Comma) { Advance(); continue; }
                        break;
                    }
                }
                if (Cur().kind != Tok::RParen) {
                    Diagnostic& d = m_diags.Error("expr.unclosed_call", "'(' 가 닫히지 않았다");
                    d.mark = MarkOf(open);
                } else {
                    Advance();
                }
                expr = std::move(node);
                continue;
            }
            break;
        }
        return expr;
    }

    ExprPtr ParsePrimary() {
        const Token t = Cur();

        switch (t.kind) {
            case Tok::Number: {
                auto e = Make(ExprKind::Literal, t);
                e->literal = t.isInteger ? Value{static_cast<i64>(t.number)} : Value{t.number};
                Advance();
                return e;
            }
            case Tok::String: {
                auto e = Make(ExprKind::Literal, t);
                e->literal = Value{t.text};
                Advance();
                return e;
            }
            case Tok::Identifier: {
                if (t.text == "true" || t.text == "false") {
                    auto e = Make(ExprKind::Literal, t);
                    e->literal = Value{t.text == "true"};
                    Advance();
                    return e;
                }
                if (t.text == "null") {
                    auto e = Make(ExprKind::Literal, t);
                    Advance();
                    return e;
                }
                auto e = Make(ExprKind::Identifier, t);
                e->name = t.text;
                Advance();
                return e;
            }
            case Tok::LParen: {
                Advance();
                ExprPtr inner = ParseBinary(0);
                if (Cur().kind != Tok::RParen) {
                    Diagnostic& d = m_diags.Error("expr.unclosed_paren", "'(' 가 닫히지 않았다");
                    d.mark = MarkOf(t);
                } else {
                    Advance();
                }
                return inner;
            }
            default: break;
        }

        Diagnostic& d = m_diags.Error(
            "expr.expected_value",
            t.kind == Tok::End ? std::string("식이 갑자기 끝났다")
                               : Fmt("값이 와야 할 자리에 '{}' 가 있다", t.text));
        d.hint = "이름, 숫자, 문자열, 또는 괄호로 시작해야 한다";
        d.mark = MarkOf(t);
        return nullptr;
    }

    std::vector<Token> m_tokens;
    DiagnosticBag&     m_diags;
    Mark               m_base;
    usize              m_index = 0;
};

void CollectNames(const Expr& expr, std::vector<const Expr*>& out) {
    if (expr.kind == ExprKind::Identifier || expr.kind == ExprKind::Member) {
        // 호출 대상이면 Call 쪽에서 처리하므로 여기서도 같이 모은다(중복 검사는 무해).
        out.push_back(&expr);
    }
    for (const ExprPtr& child : expr.children) {
        if (child) CollectNames(*child, out);
    }
}

} // namespace

// ── Expr ───────────────────────────────────────────────────────────────────
std::string Expr::QualifiedName() const {
    if (kind == ExprKind::Identifier) return name;
    if (kind == ExprKind::Member && !children.empty() && children[0]) {
        const std::string base = children[0]->QualifiedName();
        if (base.empty()) return {};
        return base + "." + name;
    }
    return {};
}

std::string Expr::ToText() const {
    switch (kind) {
        case ExprKind::Literal:
            return literal.IsString() ? ("\"" + literal.AsString() + "\"")
                                      : literal.ToScalarText();
        case ExprKind::Identifier:
            return name;
        case ExprKind::Member:
            return (children.empty() || !children[0]) ? name : children[0]->ToText() + "." + name;
        case ExprKind::Call: {
            std::string out = children.empty() || !children[0] ? "?" : children[0]->ToText();
            out += '(';
            for (usize i = 1; i < children.size(); ++i) {
                if (i > 1) out += ", ";
                out += children[i] ? children[i]->ToText() : "?";
            }
            out += ')';
            return out;
        }
        case ExprKind::Unary:
            return op + (op == "not" ? " " : "") +
                   (children.empty() || !children[0] ? "?" : children[0]->ToText());
        case ExprKind::Binary:
            return (children.size() < 2 || !children[0] || !children[1])
                       ? op
                       : children[0]->ToText() + " " + op + " " + children[1]->ToText();
    }
    return {};
}

// ── SymbolTable ────────────────────────────────────────────────────────────
void SymbolTable::Add(Symbol symbol) {
    std::string key = symbol.name;
    m_symbols[std::move(key)] = std::move(symbol);
}

void SymbolTable::AddProperty(std::string name, std::string type, std::string summary) {
    Symbol s;
    s.name    = std::move(name);
    s.kind    = Kind::Property;
    s.type    = std::move(type);
    s.summary = std::move(summary);
    Add(std::move(s));
}

void SymbolTable::AddFunction(std::string name, usize minArgs, usize maxArgs,
                              std::string type, std::string summary) {
    Symbol s;
    s.name    = std::move(name);
    s.kind    = Kind::Function;
    s.minArgs = minArgs;
    s.maxArgs = maxArgs;
    s.type    = std::move(type);
    s.summary = std::move(summary);
    Add(std::move(s));
}

const SymbolTable::Symbol* SymbolTable::Find(std::string_view name) const {
    auto it = m_symbols.find(name);
    return it == m_symbols.end() ? nullptr : &it->second;
}

std::vector<std::string> SymbolTable::Names() const {
    std::vector<std::string> names;
    names.reserve(m_symbols.size() + m_locals.size());
    for (const auto& [name, symbol] : m_symbols) names.push_back(name);
    for (const std::string& l : m_locals) names.push_back(l);
    return names;
}

void SymbolTable::AddLocal(std::string name) { m_locals.push_back(std::move(name)); }

bool SymbolTable::HasLocal(std::string_view name) const {
    return std::find(m_locals.begin(), m_locals.end(), name) != m_locals.end();
}

void SymbolTable::ClearLocals() { m_locals.clear(); }

const SymbolTable& SymbolTable::Core() {
    static SymbolTable s_table = [] {
        SymbolTable t;
        RegisterCoreSymbols(t);
        return t;
    }();
    return s_table;
}

// ── 공개 API ───────────────────────────────────────────────────────────────
bool ParseExpression(std::string_view text, ExprPtr& outExpr,
                     DiagnosticBag& diagnostics, Mark baseMark) {
    const usize before = diagnostics.ErrorCount();

    Lexer lexer(text, diagnostics, baseMark);
    Parser parser(lexer.Run(), diagnostics, baseMark);
    outExpr = parser.Run();

    return outExpr != nullptr && diagnostics.ErrorCount() == before;
}

bool CheckExpressionSymbols(const Expr& expr, const SymbolTable& symbols,
                            DiagnosticBag& diagnostics, std::string_view documentPath) {
    const usize before = diagnostics.ErrorCount();

    // 호출된 이름과 그 인자 개수를 먼저 모은다.
    std::map<std::string, usize> callArity;
    std::vector<const Expr*> stack{&expr};
    while (!stack.empty()) {
        const Expr* node = stack.back();
        stack.pop_back();
        if (node->kind == ExprKind::Call && !node->children.empty() && node->children[0]) {
            const std::string name = node->children[0]->QualifiedName();
            if (!name.empty()) callArity[name] = node->children.size() - 1;
        }
        for (const ExprPtr& child : node->children) {
            if (child) stack.push_back(child.get());
        }
    }

    std::vector<const Expr*> names;
    CollectNames(expr, names);

    for (const Expr* node : names) {
        const std::string qualified = node->QualifiedName();
        if (qualified.empty()) continue;

        // 부분 경로("input" of "input.pressed")는 그 자체로 검사하지 않는다.
        // 전체 이름이 심볼표에 있으면 통과다.
        if (symbols.Find(qualified) || symbols.HasLocal(qualified)) {
            const SymbolTable::Symbol* symbol = symbols.Find(qualified);
            auto it = callArity.find(qualified);

            if (symbol && symbol->kind == SymbolTable::Kind::Function && it == callArity.end()) {
                Diagnostic& d = diagnostics.Error(
                    "expr.function_not_called",
                    Fmt("'{}' 는 함수다. 괄호가 필요하다", qualified));
                d.hint = Fmt("예) {}({})", qualified,
                                symbol->minArgs > 0 ? "..." : "");
                d.mark = node->mark;
                d.file = std::string(documentPath);
                continue;
            }
            if (symbol && symbol->kind == SymbolTable::Kind::Property && it != callArity.end()) {
                Diagnostic& d = diagnostics.Error(
                    "expr.property_called",
                    Fmt("'{}' 는 값이다. 함수처럼 부를 수 없다", qualified));
                d.hint = Fmt("괄호를 지워라: {}", qualified);
                d.mark = node->mark;
                d.file = std::string(documentPath);
                continue;
            }
            if (symbol && it != callArity.end()) {
                const usize given = it->second;
                if (given < symbol->minArgs || given > symbol->maxArgs) {
                    Diagnostic& d = diagnostics.Error(
                        "expr.wrong_arity",
                        Fmt("'{}' 에 인자 {}개를 줬는데 {}~{}개가 필요하다",
                               qualified, static_cast<u64>(given),
                               static_cast<u64>(symbol->minArgs),
                               static_cast<u64>(symbol->maxArgs)));
                    d.hint = symbol->summary;
                    d.mark = node->mark;
                    d.file = std::string(documentPath);
                }
            }
            continue;
        }

        // 부모가 Member 인 Identifier 는 접두 조각이므로 건너뛴다.
        bool isPrefix = false;
        for (const Expr* other : names) {
            if (other == node) continue;
            const std::string otherName = other->QualifiedName();
            if (otherName.size() > qualified.size() &&
                StartsWith(otherName, qualified) &&
                otherName[qualified.size()] == '.') {
                isPrefix = true;
                break;
            }
        }
        if (isPrefix) continue;

        Diagnostic& d = diagnostics.Error("expr.unknown_name",
                                          Fmt("'{}' 는 식에서 쓸 수 있는 이름이 아니다", qualified));
        d.mark = node->mark;
        d.file = std::string(documentPath);

        const std::vector<std::string> candidates = symbols.Names();
        const std::string suggestion = ClosestMatch(qualified, candidates);
        if (!suggestion.empty()) {
            d.hint = Fmt("'{}' 을(를) 뜻했는가? ", suggestion);
        }
        d.hint += "`alice verbs --symbols` 로 쓸 수 있는 이름 전체를 볼 수 있다";
    }

    return diagnostics.ErrorCount() == before;
}

bool ValidateExpression(std::string_view text, const SymbolTable& symbols,
                        DiagnosticBag& diagnostics, Mark baseMark,
                        std::string_view documentPath) {
    ExprPtr expr;
    if (!ParseExpression(text, expr, diagnostics, baseMark)) {
        for (Diagnostic& d : diagnostics.Items()) {
            if (d.file.empty()) d.file = std::string(documentPath);
        }
        return false;
    }
    return CheckExpressionSymbols(*expr, symbols, diagnostics, documentPath);
}

} // namespace alice::verbs
