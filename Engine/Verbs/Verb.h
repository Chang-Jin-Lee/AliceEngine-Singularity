// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Verbs/Verb.h
//
// **동사(Verb)는 이 엔진의 API 표면 전부다.**
//
// 언리얼에서 게임플레이를 짜려면 C++ 클래스와 블루프린트 노드를 알아야 하고,
// 유니티라면 수천 개의 C# API 를 알아야 한다. 어느 쪽도 목록을 뽑을 수 없다.
// AI 는 결국 "아마 이런 함수가 있을 것"이라고 추측하고, 자주 틀린다.
//
// 여기서는 콘텐츠가 호출할 수 있는 모든 것이 이 레지스트리에 등록된 동사다.
//   alice verbs --json        → 전체 목록과 인자 스키마
//   alice verbs audio.play    → 그 동사 하나의 설명과 예시
//
// 목록이 유한하고 기계가 읽을 수 있다는 것 — 그게 AI 친화의 실질이다.
#pragma once

#include "Schema/Registry.h"

#include <map>
#include <string>
#include <vector>

namespace alice::verbs {

using schema::SchemaPtr;
using doc::Value;

struct Verb {
    std::string id;            ///< "audio.play". 소문자 점 표기
    std::string summary;       ///< 한 줄
    std::string description;   ///< 여러 줄 설명 (선택)
    SchemaPtr   args;          ///< 인자 맵 스키마. 인자가 없으면 nullptr

    /// 이 동사가 무엇에 영향을 주는지. 충돌 검사와 문서 분류에 쓴다.
    std::vector<std::string> tags;

    /// 올바른 사용 예. 위키와 AI 프롬프트에 그대로 들어간다.
    std::vector<Value> examples;

    /// 같은 입력이면 항상 같은 결과인가.
    /// 리플레이·결정적 테스트에서 비결정적 동사를 걸러내는 데 쓴다.
    bool deterministic = true;

    /// 프레임당 호출 비용의 대략적 등급. 예산 경고의 근거가 된다.
    enum class Cost : u8 { Trivial, Cheap, Moderate, Expensive };
    Cost cost = Cost::Cheap;
};

const char* ToString(Verb::Cost cost) noexcept;

class VerbRegistry {
public:
    void Register(Verb verb);

    const Verb* Find(std::string_view id) const;
    bool        Has(std::string_view id) const;
    usize       Size() const noexcept { return m_byId.size(); }

    std::vector<std::string> Ids() const;
    /// tag 로 거른 목록. 비우면 전부.
    std::vector<const Verb*> ByTag(std::string_view tag) const;

    /// **AI 가 읽는 전체 API 레퍼런스.** 한 번의 호출로 엔진이 할 수 있는 전부를 준다.
    std::string ToJson(const schema::Registry& schemas) const;

    void Clear();

    static VerbRegistry& Global();

private:
    std::map<std::string, Verb, std::less<>> m_byId;
};

/// 엔진 기본 동사들을 등록한다.
void RegisterCoreVerbs(VerbRegistry& registry);

} // namespace alice::verbs
