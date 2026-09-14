// SPDX-License-Identifier: MIT
#include "Registry.h"

#include "Doc/Document.h"
#include "Foundation/Log.h"
#include "Foundation/StringUtil.h"

namespace alice::schema {
namespace {
constexpr const char* kChannel = "schema";
}

void Registry::Register(SchemaPtr schema) {
    if (!schema || schema->id.empty()) return;

    auto it = m_byId.find(schema->id);
    if (it != m_byId.end()) {
        // 조용한 덮어쓰기는 추적이 불가능하다. 크게 소리내고 새것을 쓴다.
        ALICE_LOG_ERROR(kChannel, "schema.duplicate_id")
            .Msg("같은 스키마 id 가 두 번 등록됐다")
            .F("id", schema->id);
        it->second = std::move(schema);
        return;
    }
    m_byId.emplace(schema->id, std::move(schema));
}

SchemaPtr Registry::Find(std::string_view id) const {
    auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : it->second;
}

SchemaPtr Registry::FindLatest(std::string_view baseId) const {
    SchemaPtr best;
    u32       bestVersion = 0;

    for (const auto& [id, schema] : m_byId) {
        std::string base;
        u32         version = 0;
        if (!doc::Document::SplitSchemaId(id, base, version)) continue;
        if (base != baseId) continue;
        if (version >= bestVersion) { bestVersion = version; best = schema; }
    }
    return best;
}

SchemaPtr Registry::Resolve(const Schema& schema) const {
    if (schema.type != Type::Ref) return nullptr;
    return Find(schema.refId);
}

bool Registry::Has(std::string_view id) const { return m_byId.find(id) != m_byId.end(); }

std::vector<std::string> Registry::Ids() const {
    std::vector<std::string> ids;
    ids.reserve(m_byId.size());
    for (const auto& [id, schema] : m_byId) ids.push_back(id);
    return ids;   // std::map 이라 이미 사전순이다
}

std::vector<SchemaPtr> Registry::All() const {
    std::vector<SchemaPtr> all;
    all.reserve(m_byId.size());
    for (const auto& [id, schema] : m_byId) all.push_back(schema);
    return all;
}

void Registry::Clear() { m_byId.clear(); }

Registry& Registry::Global() {
    static Registry s_registry = [] {
        Registry r;
        RegisterCoreSchemas(r);
        return r;
    }();
    return s_registry;
}

} // namespace alice::schema
