// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Doc/JsonParser.h
//
// JSON 은 이 엔진에서 **도구 사이의 교환 형식**이다.
// 사람과 AI 는 YAML 로 쓰고, 엔진↔CLI↔에디터↔외부 AI 사이에는 JSON 이 흐른다.
// 둘은 같은 Value 로 들어오므로 어느 쪽으로 써도 결과가 같다.
//
// 표준 JSON 을 따르되 두 가지를 옵션으로 연다:
//   • 주석 (// 와 /* */)  — 설정 파일에 설명을 남길 수 있어야 한다
//   • 마지막 쉼표          — 생성 코드가 만들기 쉽다
// 기본값은 둘 다 켜져 있다. 엄격한 JSON 만 받으려면 옵션을 끄면 된다.
#pragma once

#include "Value.h"

namespace alice::doc {

struct JsonParseOptions {
    bool allowComments      = true;
    bool allowTrailingComma = true;
    bool duplicateKeyIsError = true;
    u32  maxDepth           = 64;
};

bool ParseJson(std::string_view text,
               Value& outRoot,
               DiagnosticBag& diagnostics,
               const JsonParseOptions& options = {});

} // namespace alice::doc
