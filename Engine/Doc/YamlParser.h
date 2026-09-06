// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Doc/YamlParser.h
//
// **YAML 전체가 아니라 부분집합**을 파싱한다. 이건 타협이 아니라 설계 결정이다.
//
// 왜 서드파티 YAML 라이브러리를 안 쓰는가:
//   1) 이 엔진에서 파서의 에러 메시지는 **제품 기능**이다. 콘텐츠를 AI가 쓰기 때문에,
//      "line 12: error" 같은 일반적 메시지로는 자가수정이 안 된다. 어떤 키가 틀렸는지,
//      뭐라고 고쳐야 하는지까지 파서가 알아야 한다. 남의 파서는 그걸 못 준다.
//   2) YAML 1.2 전체는 앵커·별칭·태그·복합키·다중문서까지 포함해서 사람도 헷갈린다.
//      게임 콘텐츠에 필요 없는 기능이 대부분이고, 있으면 AI 가 헷갈릴 여지만 는다.
//   3) 의존성 0. 클론하면 바로 빌드된다.
//
// 지원한다:
//   블록 맵 / 블록 시퀀스 / 들여쓰기 중첩
//   플로우 시퀀스 [a, b] / 플로우 맵 {a: 1}
//   평문·홑따옴표·겹따옴표 스칼라 (겹따옴표는 이스케이프 포함)
//   블록 스칼라  | (literal) 과 > (folded), 청킹 지시자 -/+
//   주석 #
//   문서 시작 마커 ---
//   타입 자동 판별: null ~ true false 정수 실수 문자열
//
// 지원하지 않는다 (전부 **명확한 에러와 대안 안내**를 낸다):
//   앵커 &x / 별칭 *x   → 문서 재사용은 $ref 로 한다
//   태그 !!str          → 스키마가 타입을 정한다
//   복합키 ? key        → 키는 항상 문자열이다
//   다중 문서 (--- 반복) → 파일 하나에 문서 하나
//   탭 들여쓰기          → 공백만
#pragma once

#include "Value.h"

namespace alice::doc {

struct YamlParseOptions {
    /// 중복 키를 에러로 볼지. 기본 true — 조용히 덮어쓰면 콘텐츠 버그가 숨는다.
    bool duplicateKeyIsError = true;
    /// 최대 중첩 깊이. 악의적/실수로 만들어진 깊은 문서에서 스택을 지킨다.
    u32 maxDepth = 64;
};

/// YAML 텍스트를 파싱한다. 실패해도 가능한 만큼 파싱해서 돌려준다
/// (부분 결과 + 여러 진단 → AI 가 한 번에 여러 곳을 고칠 수 있다).
/// 반환값: 에러가 하나도 없으면 true.
bool ParseYaml(std::string_view text,
               Value& outRoot,
               DiagnosticBag& diagnostics,
               const YamlParseOptions& options = {});

} // namespace alice::doc
