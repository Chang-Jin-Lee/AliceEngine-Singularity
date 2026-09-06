// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Doc/Writer.h
//
// 값을 다시 텍스트로 쓴다. 이 방향이 **엔진의 핵심 기능**이다.
//
// 왜: 언리얼/유니티에서 AI 가 콘텐츠를 다루기 어려운 진짜 이유는 "읽기"가 아니라
// "현재 상태를 되받아 읽을 수 없어서"다. 상태가 바이너리 애셋과 C++ 객체 안에 있으면
// AI 는 자기가 뭘 바꿨는지 확인할 방법이 없다.
//
// 이 엔진은 반대다. 런타임 상태를 언제든 같은 문서 형식으로 덤프할 수 있다.
//   읽기 → 고치기 → 되쓰기 → 다시 읽기 가 완전히 닫힌 고리를 이룬다.
// 그래서 왕복(round-trip) 정확성이 테스트로 강제된다.
#pragma once

#include "Value.h"

namespace alice::doc {

struct YamlWriteOptions {
    u32  indentWidth  = 2;
    /// 짧은 숫자 시퀀스를 [1, 2, 3] 한 줄로 쓴다. 벡터/색상이 훨씬 읽기 좋아진다.
    bool compactNumberSeq = true;
    u32  compactSeqMaxItems = 6;
    /// 여러 줄 문자열을 블록 스칼라(|)로 쓴다. 끄면 "\n" 이스케이프로 나간다.
    bool useBlockScalars = true;

    /// 짧고 스칼라만 있는 맵을 `{a: 1, b: 2}` 한 줄로 쓴다.
    /// 동작 인자처럼 두세 개짜리 맵이 두 줄씩 차지하면 규칙 목록이 금세 안 읽힌다.
    bool compactScalarMap = true;
    u32  compactMapMaxEntries = 3;
    /// 한 줄로 접었을 때 이 길이를 넘으면 접지 않는다.
    u32  compactLineLimit = 76;
};

struct JsonWriteOptions {
    bool pretty      = true;
    u32  indentWidth = 2;
    /// 키를 사전순으로 정렬한다. 기본은 **끄기** — 문서의 키 순서는 의미 있는 정보다.
    bool sortKeys = false;
};

std::string ToYaml(const Value& value, const YamlWriteOptions& options = {});
std::string ToJson(const Value& value, const JsonWriteOptions& options = {});

/// 평문으로 써도 안전한 문자열인지. 파서와 짝을 이루는 판정이라 여기 노출한다.
bool NeedsYamlQuoting(std::string_view s) noexcept;

} // namespace alice::doc
