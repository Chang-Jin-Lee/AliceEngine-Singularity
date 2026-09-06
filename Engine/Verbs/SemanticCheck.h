// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Verbs/SemanticCheck.h
//
// 스키마 검증 다음의 **2차 검사**.
//
// 스키마는 "구조가 맞는가"까지만 본다. `do:` 가 맵의 목록이라는 것은 알지만,
// 그 맵의 키가 실제로 존재하는 동사인지, 인자가 그 동사의 스키마에 맞는지는 모른다.
// `when:` 이 문자열이라는 것은 알지만 그 안의 이름이 실재하는지는 모른다.
//
// 그 나머지를 여기서 본다. 이 두 단계를 통과하면 문서는 **실행 전에** 다음을 보장받는다:
//   • 모든 필드가 존재하고 타입이 맞다
//   • 모든 동사가 실재하고 인자가 맞다
//   • 모든 조건식이 문법에 맞고 이름이 실재한다
//
// 언리얼/유니티에서 이 정도 확신을 얻으려면 컴파일하고 실행해서 그 코드 경로를 밟아야 한다.
// 여기서는 `alice check` 한 번이면 된다. AI 의 반복 비용이 그만큼 줄어든다.
#pragma once

#include "Expression.h"
#include "Verb.h"

namespace alice::verbs {

struct SemanticCheckOptions {
    /// 조건식 안의 이름을 검사할지. 콘텐츠가 아직 변수를 선언하기 전이면 끌 수 있다.
    bool checkExpressionSymbols = true;
    /// 동사 인자를 스키마로 검사할지.
    bool checkVerbArguments = true;
};

/// 문서 전체를 훑으며 `when:` 식과 `do:` 동작을 검사한다.
/// 문서 타입에 무관하게 동작한다 — behavior 든 animation 이든 같은 규칙이 걸린다.
bool CheckDocumentSemantics(const doc::Value&        root,
                            const VerbRegistry&      verbs,
                            const schema::Registry&  schemas,
                            DiagnosticBag&           diagnostics,
                            std::string_view         documentPath = {},
                            const SemanticCheckOptions& options = {});

/// 동작 하나( { "<동사>": {...} } )를 검사한다.
bool CheckAction(const doc::Value&       action,
                 const VerbRegistry&     verbs,
                 const schema::Registry& schemas,
                 DiagnosticBag&          diagnostics,
                 std::string_view        path,
                 const SemanticCheckOptions& options = {});

} // namespace alice::verbs
