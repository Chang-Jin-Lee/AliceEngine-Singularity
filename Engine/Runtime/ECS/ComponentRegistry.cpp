// SPDX-License-Identifier: MIT
// Freeze schema bindings before pools borrow descriptors or instantiate native components.
#include "Runtime/ComponentRegistry.h"
#include "Runtime/ECS/Support.h"
#include "Schema/Registry.h"
#include "Schema/Validator.h"
#include <unordered_set>

namespace alice::runtime {
struct ComponentRegistry::Impl {
    struct Entry { ComponentDescriptor descriptor; SourceLocation source; };
    u64 identity = ecs::FreshIdentity();
    std::vector<Entry> entries;
    std::shared_ptr<const schema::Registry> schemas;
};

ComponentRegistry::ComponentRegistry() : m_impl(std::make_unique<Impl>()) {}
ComponentRegistry::~ComponentRegistry() = default;
bool ComponentRegistry::IsFrozen() const noexcept { return m_impl->schemas != nullptr; }

Result<ComponentId> ComponentRegistry::Register(ComponentDescriptor descriptor, const SourceLocation& source) {
    if (IsFrozen()) return ecs::Error("runtime.registry.frozen", "component registry is frozen",
        "create a new registry before changing component bindings", source);
    const auto& d = descriptor;
    if (d.name.empty() || d.schemaId.empty() || d.nativeType == typeid(void) || d.size == 0 ||
        d.alignment == 0 || (d.alignment & (d.alignment - 1)) != 0 || d.size % d.alignment != 0 ||
        !d.construct || !d.destroy || !d.moveConstruct || !d.decode || !d.encode)
        return ecs::Error("runtime.registry.invalid_descriptor", "component descriptor is incomplete or misaligned",
            "provide name, schema ID, native type, size/alignment and all five lifecycle/codec callbacks", source);
    for (const auto& entry : m_impl->entries) {
        const auto& existing = entry.descriptor;
        if (existing.name == d.name || existing.schemaId == d.schemaId || existing.nativeType == d.nativeType)
            return ecs::Error("runtime.registry.duplicate", "component key, schema or native type is already registered",
                "reuse the existing ComponentId or register a distinct binding", source);
    }
    if (m_impl->entries.size() >= std::numeric_limits<u32>::max())
        return ecs::Error("runtime.storage.capacity", "component ID space is exhausted", "use a new registry", source);
    m_impl->entries.push_back({std::move(descriptor), source});
    return ComponentId{m_impl->identity, static_cast<u32>(m_impl->entries.size())};
}

Status ComponentRegistry::Freeze(std::shared_ptr<const schema::Registry> schemas) {
    if (IsFrozen()) return ecs::Error("runtime.registry.frozen", "component registry is already frozen",
        "reuse this registry or create a new one");
    if (!schemas) return ecs::Error("runtime.registry.schema_unresolved", "schema registry is null",
        "provide a registry containing all component schemas and referenced schemas");
    for (const auto& entry : m_impl->entries) {
        std::vector<schema::SchemaPtr> pending{schemas->Find(entry.descriptor.schemaId)};
        std::unordered_set<const schema::Schema*> visited;
        while (!pending.empty()) {
            auto node = std::move(pending.back()); pending.pop_back();
            if (!node) return ecs::Error("runtime.registry.schema_unresolved", "component schema or reference is missing",
                "register the component schema and all of its referenced schemas before Freeze", entry.source,
                "components." + entry.descriptor.name);
            if (!visited.insert(node.get()).second) continue;
            if (node->type == schema::Type::Ref) pending.push_back(schemas->Find(node->refId));
            for (const auto& field : node->fields) pending.push_back(field.type);
            for (const auto& option : node->options) pending.push_back(option);
            if (node->items) pending.push_back(node->items);
            if (node->additionalFieldType) pending.push_back(node->additionalFieldType);
        }
    }
    m_impl->schemas = std::move(schemas);
    return Status::Ok();
}

Result<ComponentId> ComponentRegistry::Find(std::string_view name) const {
    for (usize i = 0; i < m_impl->entries.size(); ++i)
        if (m_impl->entries[i].descriptor.name == name) return ComponentId{m_impl->identity, static_cast<u32>(i + 1)};
    return ecs::Error("runtime.component.unknown", Fmt("component '{}' is not registered", name),
        "register this component before Freeze or use an existing component key", {}, "components." + std::string(name));
}

Result<const ComponentDescriptor*> ComponentRegistry::Describe(ComponentId component) const {
    if (component.registry != m_impl->identity || component.index == 0 || component.index > m_impl->entries.size())
        return ecs::Error("runtime.component.unknown", "component ID belongs to no entry in this registry",
            "look up the component key with Find on this registry", {}, Fmt("components[{}:{}]", component.registry, component.index));
    return &m_impl->entries[component.index - 1].descriptor;
}

Status ComponentRegistry::Validate(ComponentId component, const doc::Value& document,
                                   DiagnosticBag& diagnostics, const SourceLocation& source) const {
    DiagnosticBag added;
    auto descriptor = Describe(component);
    if (!IsFrozen()) added.Add(ecs::Error("runtime.registry.not_frozen", "component registry is not frozen",
        "call Freeze with all referenced schemas before validation", source));
    else if (!descriptor) {
        auto error = descriptor.Error(); error.path.clear();
        added.Add(ecs::Located(std::move(error), source, document.mark));
    } else {
        const auto schema = m_impl->schemas->Find(descriptor.Value()->schemaId);
        // External aliases must keep the retained schema graph unchanged; fail safely if violated.
        if (!schema) added.Add(ecs::Error("runtime.registry.schema_unresolved", "frozen component schema is missing",
            "restore the retained schema registry; do not mutate it after Freeze", source));
        else {
            // A diagnostic display limit must not let unvalidated data reach native codecs.
            schema::ValidateOptions options;
            options.maxDiagnostics = std::numeric_limits<usize>::max();
            schema::Validate(document, *schema, *m_impl->schemas, added, options,
                source.path.empty() ? "components." + descriptor.Value()->name : source.path);
        }
    }
    Status result;
    for (auto& diagnostic : added.Items()) {
        diagnostic = ecs::Located(std::move(diagnostic), source, document.mark);
        if (diagnostic.severity == Severity::Error && result.IsOk()) result = Status(diagnostic);
        diagnostics.Add(std::move(diagnostic));
    }
    return result;
}
} // namespace alice::runtime
