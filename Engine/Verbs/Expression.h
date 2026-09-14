// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Verbs/Expression.h
//
// 조건식. behavior 문서의 `when:` 과 animation 문서의 전이 조건이 이 문법을 쓴다.
//
// 이것이 이 엔진에서 "코드"에 가장 가까운 것이고, 의도적으로 **여기서 멈춘다.**
// 루프도, 대입도, 함수 정의도 없다. 식은 상태를 읽어 참/거짓이나 수를 낼 뿐이다.
// 상태를 바꾸는 일은 전부 동사(Verb)가 한다.
//
// 왜 이렇게 좁히는가:
//   • 부작용 없는 식은 순서에 의존하지 않는다 → 규칙을 어떤 순서로 평가해도 같다
//   • 정적으로 전부 검사할 수 있다 → AI 가 쓴 조건의 오타를 실행 전에 잡는다
//   • 사람이 읽을 수 있다 → 리뷰가 가능하다
//
//   input.pressed("Jump") and physics.grounded and not self.stunned
//   health / maxHealth < 0.3
//   time.since(lastHit) > 1.5
//
// 파서는 AST 만 만든다. 평가는 Runtime 모듈의 몫이다(백로그 SID-04).
#pragma once

#include "Foundation/Diagnostic.h"
#include "Doc/Value.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace alice::verbs {

using doc::Value;

enum class ExprKind : u8 {
    Literal,      ///< 3, 1.5, "text", true, null
    Identifier,   ///< health
    Member,       ///< input.pressed  (children[0] 이 좌변, name 이 우변)
    Call,         ///< f(a, b)        (children[0] 이 대상, 나머지가 인자)
    Unary,        ///< not x, -x
    Binary,       ///< a + b, a and b
};

struct Expr;
using ExprPtr = std::shared_ptr<Expr>;

struct Expr {
    ExprKind             kind = ExprKind::Literal;
    Value                literal;      ///< Literal
    std::string          name;         ///< Identifier / Member
    std::string          op;           ///< Unary / Binary
    std::vector<ExprPtr> children;
    Mark                 mark;

    /// "input.pressed" 처럼 점으로 이어진 이름을 복원한다. 그렇지 않으면 빈 문자열.
    std::string QualifiedName() const;
    /// 사람이 읽을 수 있게 되돌린다(진단과 위키에 쓴다).
    std::string ToText() const;
};

/// 식에서 쓸 수 있는 이름의 목록.
/// **이 표가 없으면 AI 는 이름을 지어내고, 우리는 런타임까지 그걸 모른다.**
class SymbolTable {
public:
    enum class Kind : u8 { Property, Function };

    struct Symbol {
        std::string name;          ///< "input.pressed"
        Kind        kind = Kind::Property;
        usize       minArgs = 0;
        usize       maxArgs = 0;
        std::string type;          ///< "bool", "float", "vec3" — 문서용
        std::string summary;
    };

    void Add(Symbol symbol);
    void AddProperty(std::string name, std::string type, std::string summary);
    void AddFunction(std::string name, usize minArgs, usize maxArgs,
                     std::string type, std::string summary);

    const Symbol* Find(std::string_view name) const;
    std::vector<std::string> Names() const;
    usize Size() const noexcept { return m_symbols.size(); }

    /// 콘텐츠가 선언한 지역 변수(behavior 의 variables, animation 의 parameters).
    /// 검증할 때마다 달라지므로 별도로 얹는다.
    void AddLocal(std::string name);
    bool HasLocal(std::string_view name) const;
    void ClearLocals();

    /// 엔진 기본 심볼이 등록된 전역 표.
    static const SymbolTable& Core();

private:
    std::map<std::string, Symbol, std::less<>> m_symbols;
    std::vector<std::string>                   m_locals;
};

void RegisterCoreSymbols(SymbolTable& table);

/// 식을 파싱한다. baseMark 는 이 식이 문서 어디에 있었는지(줄/열)로, 진단 위치의 기준이 된다.
bool ParseExpression(std::string_view text,
                     ExprPtr&         outExpr,
                     DiagnosticBag&   diagnostics,
                     Mark             baseMark = {});

/// 파싱된 식의 이름들이 실제로 존재하는지 확인한다. 오타에는 제안을 붙인다.
bool CheckExpressionSymbols(const Expr&        expr,
                            const SymbolTable& symbols,
                            DiagnosticBag&     diagnostics,
                            std::string_view   documentPath = {});

/// 파싱 + 심볼 검사를 한 번에.
bool ValidateExpression(std::string_view   text,
                        const SymbolTable& symbols,
                        DiagnosticBag&     diagnostics,
                        Mark               baseMark = {},
                        std::string_view   documentPath = {});

} // namespace alice::verbs
