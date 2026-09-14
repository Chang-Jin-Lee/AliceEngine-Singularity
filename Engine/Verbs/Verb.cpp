// SPDX-License-Identifier: MIT
#include "Verb.h"

#include "Doc/JsonParser.h"
#include "Doc/Writer.h"
#include "Foundation/Log.h"
#include "Foundation/StringUtil.h"
#include "Schema/JsonSchemaEmitter.h"

namespace alice::verbs {
namespace {
constexpr const char* kChannel = "verbs";
}

const char* ToString(Verb::Cost cost) noexcept {
    switch (cost) {
        case Verb::Cost::Trivial:   return "trivial";
        case Verb::Cost::Cheap:     return "cheap";
        case Verb::Cost::Moderate:  return "moderate";
        case Verb::Cost::Expensive: return "expensive";
    }
    return "cheap";
}

void VerbRegistry::Register(Verb verb) {
    if (verb.id.empty()) {
        ALICE_LOG_ERROR(kChannel, "verb.empty_id").Msg("id 없는 동사를 등록하려 했다");
        return;
    }
    auto it = m_byId.find(verb.id);
    if (it != m_byId.end()) {
        ALICE_LOG_ERROR(kChannel, "verb.duplicate_id")
            .Msg("같은 동사 id 가 두 번 등록됐다")
            .F("id", verb.id);
        it->second = std::move(verb);
        return;
    }
    m_byId.emplace(verb.id, std::move(verb));
}

const Verb* VerbRegistry::Find(std::string_view id) const {
    auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : &it->second;
}

bool VerbRegistry::Has(std::string_view id) const { return m_byId.find(id) != m_byId.end(); }

std::vector<std::string> VerbRegistry::Ids() const {
    std::vector<std::string> ids;
    ids.reserve(m_byId.size());
    for (const auto& [id, verb] : m_byId) ids.push_back(id);
    return ids;
}

std::vector<const Verb*> VerbRegistry::ByTag(std::string_view tag) const {
    std::vector<const Verb*> out;
    for (const auto& [id, verb] : m_byId) {
        if (tag.empty()) { out.push_back(&verb); continue; }
        for (const std::string& t : verb.tags) {
            if (t == tag) { out.push_back(&verb); break; }
        }
    }
    return out;
}

std::string VerbRegistry::ToJson(const schema::Registry& schemas) const {
    Value root = Value::MakeMap();
    root.Set("engine", Value{"AliceEngine-Singularity"});
    root.Set("count", Value{static_cast<i64>(m_byId.size())});
    root.Set("note",
             Value{"콘텐츠 문서의 do: 목록에서 쓸 수 있는 동사 전부. "
                   "각 항목의 args 는 JSON Schema 이다"});

    Value list = Value::MakeSeq();
    for (const auto& [id, verb] : m_byId) {
        Value item = Value::MakeMap();
        item.Set("id", Value{verb.id});
        item.Set("summary", Value{verb.summary});
        if (!verb.description.empty()) item.Set("description", Value{verb.description});

        Value tags = Value::MakeSeq();
        for (const std::string& t : verb.tags) tags.Push(Value{t});
        item.Set("tags", std::move(tags));

        item.Set("deterministic", Value{verb.deterministic});
        item.Set("cost", Value{ToString(verb.cost)});

        if (verb.args) {
            // 인자 스키마는 표준 JSON Schema 로 낸다. 바깥 도구가 그대로 쓸 수 있게.
            doc::Value parsed;
            DiagnosticBag ignored;
            const std::string json = schema::EmitJsonSchema(*verb.args, schemas);
            if (doc::ParseJson(json, parsed, ignored)) {
                item.Set("args", std::move(parsed));
            }
        } else {
            item.Set("args", Value{});
        }

        if (!verb.examples.empty()) {
            Value examples = Value::MakeSeq();
            for (const Value& e : verb.examples) examples.Push(e);
            item.Set("examples", std::move(examples));
        }

        list.Push(std::move(item));
    }
    root.Set("verbs", std::move(list));
    return doc::ToJson(root);
}

void VerbRegistry::Clear() { m_byId.clear(); }

VerbRegistry& VerbRegistry::Global() {
    static VerbRegistry s_registry = [] {
        VerbRegistry r;
        RegisterCoreVerbs(r);
        return r;
    }();
    return s_registry;
}

} // namespace alice::verbs
