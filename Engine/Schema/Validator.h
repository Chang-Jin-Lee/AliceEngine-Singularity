// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Schema/Validator.h
//
// 검증기의 목표는 "거부"가 아니라 **수정 가능하게 만들기**다.
//
// 보통의 검증기는 "필드 'positon' 은 허용되지 않습니다" 라고 말하고 끝난다.
// 그러면 AI 는 스키마 전문을 다시 읽어야 하고, 대개 다른 곳을 고친다.
// 여기서는 이렇게 말한다:
//
//   player.actor.yaml:12:5: error[schema.unknown_field]: 'positon' 은 transform 에 없다
//     |       positon: [0, 1, 0]
//     |       ^
//     = hint: 'position' 을 뜻한 것인가? 쓸 수 있는 필드: position, rotation, scale
//
// 이 한 덩어리(위치 + 코드 + 오타 제안 + 후보 목록)가 AI 한 번의 수정으로 이어진다.
// 그래서 진단의 품질이 이 엔진의 성능 지표다.
#pragma once

#include "Foundation/Diagnostic.h"
#include "Schema.h"

namespace alice::schema {

class Registry;

struct ValidateOptions {
    /// 스키마에 없는 필드를 어떻게 볼지. Schema::additionalFields 보다 우선한다(강제 완화용).
    bool allowUnknownFields = false;
    /// deprecated 필드를 경고로 낼지. 끄면 조용히 통과한다.
    bool warnDeprecated = true;
    /// 한 번에 보고할 최대 진단 수. 넘으면 멈춘다(폭주 방지).
    usize maxDiagnostics = 200;
};

/// value 를 schema 에 대해 검증한다. 에러가 없으면 true.
/// rootPath 는 진단의 path 접두사다(문서 전체를 검증할 땐 비워 둔다).
bool Validate(const Value&    value,
              const Schema&   schema,
              const Registry& registry,
              DiagnosticBag&  diagnostics,
              const ValidateOptions& options = {},
              std::string_view rootPath = {});

/// 스키마의 기본값을 채운 사본을 만든다.
/// 왜 필요한가: AI 가 최소한의 문서만 쓰고도 완전한 상태를 얻게 하기 위해서다.
/// 문서에는 name 만 적혀 있어도 런타임은 전체 필드를 본다.
Value ApplyDefaults(const Value& value, const Schema& schema, const Registry& registry);

/// 문자열이 해당 형식에 맞는지. 맞지 않으면 이유를 outReason 에 채운다.
bool CheckFormat(TextFormat format, std::string_view text, std::string& outReason);

} // namespace alice::schema
