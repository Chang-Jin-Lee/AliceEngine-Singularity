// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Schema/Registry.h
//
// 스키마 id → 스키마. 참조($ref)를 푸는 곳이고, **AI 가 엔진에게 "무엇을 쓸 수 있냐"고
// 물었을 때 답하는 창구**다.
//
//   alice schema list          → 이 엔진이 아는 모든 문서 타입
//   alice schema show <id>     → 그 타입의 필드·설명·예시
//   alice schema emit          → JSON Schema 일괄 생성 (IDE 자동완성용)
//
// 언리얼에서 "이 클래스에 어떤 프로퍼티가 있지?"를 알려면 헤더를 읽거나 에디터를 켜야 한다.
// 여기서는 명령 한 줄이면 된다. 그게 이 레지스트리가 존재하는 이유다.
#pragma once

#include "Schema.h"

#include <map>
#include <string>
#include <vector>

namespace alice::schema {

class Registry {
public:
    /// 스키마를 등록한다. id 가 비어 있으면 무시된다.
    /// 같은 id 를 두 번 등록하면 어서트한다 — 조용한 덮어쓰기는 추적이 불가능하다.
    void Register(SchemaPtr schema);

    /// "alice/actor/1" 로 정확히 찾는다.
    SchemaPtr Find(std::string_view id) const;

    /// 버전 없이 "alice/actor" 로 찾으면 가장 높은 버전을 준다.
    SchemaPtr FindLatest(std::string_view baseId) const;

    /// Ref 를 따라가 실제 스키마를 얻는다. 못 찾으면 nullptr.
    SchemaPtr Resolve(const Schema& schema) const;

    bool  Has(std::string_view id) const;
    usize Size() const noexcept { return m_byId.size(); }

    /// 등록된 모든 id (사전순).
    std::vector<std::string> Ids() const;
    std::vector<SchemaPtr>   All() const;

    void Clear();

    /// 엔진 기본 스키마가 모두 등록된 전역 레지스트리.
    /// 첫 호출에서 RegisterCoreSchemas() 가 실행된다.
    static Registry& Global();

private:
    std::map<std::string, SchemaPtr, std::less<>> m_byId;
};

/// 엔진이 기본으로 아는 문서 타입들을 등록한다.
/// 여기 있는 목록이 곧 "이 엔진으로 만들 수 있는 것"의 정의다.
void RegisterCoreSchemas(Registry& registry);

} // namespace alice::schema
