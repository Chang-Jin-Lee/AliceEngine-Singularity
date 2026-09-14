// SPDX-License-Identifier: MIT
// Read leases own the backing state and exclude mutations; entity slots outlive dense pool reshuffles.
#include "Runtime/World.h"
#include "Runtime/ECS/NativePool.h"
#include "Runtime/ECS/Support.h"
#include "Foundation/Profiler.h"
#include <algorithm>
#include <map>

namespace alice::runtime {
namespace ecs {
struct WorldState {
    struct Slot { u32 generation = 1; bool alive = false; bool pending = false; };
    u64 identity = FreshIdentity();
    std::shared_ptr<const ComponentRegistry> registry;
    std::vector<Slot> slots{{0, false, false}};
    std::vector<u32> freeSlots;
    std::vector<u32> pending;
    std::map<u32, std::unique_ptr<NativePool>> pools;
    usize readers = 0;
    bool mutating = false;

    bool Alive(EntityId entity) const noexcept {
        return entity.world == identity && entity.index != 0 && entity.index < slots.size() &&
            slots[entity.index].alive && slots[entity.index].generation == entity.generation;
    }
    Status CheckEntity(EntityId entity, const SourceLocation& source = {}) const {
        if (Alive(entity)) return Status::Ok();
        return Error("runtime.entity.invalid", "entity is stale, destroyed or belongs to another World",
            "query a live EntityId from this World before accessing it", source,
            Fmt("entities[{}:{}:{}]", entity.world, entity.index, entity.generation));
    }
    Status CanWrite(const SourceLocation& source = {}) const {
        if (readers != 0) return Error("runtime.world.read_locked", "World has active read leases",
            "release every WorldReadView before changing the World", source, "world");
        if (mutating) return Error("runtime.world.reentrant", "a component callback re-entered World",
            "finish the callback before calling another World operation", source, "world");
        return Status::Ok();
    }
    Result<const ComponentDescriptor*> Describe(ComponentId component, const SourceLocation& source = {}) const {
        auto result = registry->Describe(component);
        if (!result) {
            auto error = result.Error();
            if (!source.path.empty()) error.path.clear();
            return Located(std::move(error), source);
        }
        return result.Value();
    }
    NativePool* Pool(ComponentId component) const {
        const auto it = pools.find(component.index);
        return it == pools.end() ? nullptr : it->second.get();
    }
    Diagnostic Missing(EntityId entity, ComponentId component, const SourceLocation& source = {}) const {
        return Error("runtime.component.missing", "entity has no instance of this component",
            "add the component with SetComponent before accessing or removing it", source,
            Fmt("entities[{}:{}:{}].components[{}]", entity.world, entity.index, entity.generation, component.index));
    }
};

struct Mutation {
    WorldState& state;
    explicit Mutation(WorldState& value) : state(value) { state.mutating = true; }
    ~Mutation() { state.mutating = false; }
    Mutation(const Mutation&) = delete;
    Mutation& operator=(const Mutation&) = delete;
};
} // namespace ecs

struct World::Impl : ecs::WorldState {};
struct WorldReadView::Impl {
    std::shared_ptr<ecs::WorldState> state;
    explicit Impl(std::shared_ptr<ecs::WorldState> value) : state(std::move(value)) { ++state->readers; }
    ~Impl() { --state->readers; }
};

World::World(std::shared_ptr<Impl> impl) : m_impl(std::move(impl)) {}
World::~World() = default;
WorldReadView::WorldReadView(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
WorldReadView::~WorldReadView() = default;
WorldReadView::WorldReadView(WorldReadView&&) noexcept = default;
WorldReadView& WorldReadView::operator=(WorldReadView&&) noexcept = default;

Result<std::unique_ptr<World>> World::Create(std::shared_ptr<const ComponentRegistry> registry) {
    if (!registry || !registry->IsFrozen()) return ecs::Error("runtime.registry.not_frozen",
        "World requires a frozen component registry", "register component bindings and call Freeze before World::Create");
    auto impl = std::make_shared<Impl>(); impl->registry = std::move(registry);
    return std::unique_ptr<World>(new World(std::move(impl)));
}

Result<WorldReadView> World::AcquireRead() const {
    if (m_impl->mutating) return ecs::Error("runtime.world.reentrant", "cannot read during a component callback",
        "acquire a read lease after the current World operation returns");
    return WorldReadView(std::make_unique<WorldReadView::Impl>(m_impl));
}

Result<EntityId> World::CreateEntity(const SourceLocation& source) {
    ALICE_TRY(m_impl->CanWrite(source));
    ecs::Mutation mutation(*m_impl);
    u32 index = 0;
    if (!m_impl->freeSlots.empty()) {
        index = m_impl->freeSlots.back(); m_impl->freeSlots.pop_back();
    } else {
        if (m_impl->slots.size() > std::numeric_limits<u32>::max()) return ecs::Error("runtime.storage.capacity",
            "entity index space is exhausted", "destroy unused entities or use a new World", source);
        index = static_cast<u32>(m_impl->slots.size()); m_impl->slots.emplace_back();
    }
    auto& slot = m_impl->slots[index]; slot.alive = true; slot.pending = false;
    return EntityId{m_impl->identity, index, slot.generation};
}

Status World::DestroyEntity(EntityId entity, const SourceLocation& source) {
    ALICE_TRY(m_impl->CanWrite(source)); ALICE_TRY(m_impl->CheckEntity(entity, source));
    ecs::Mutation mutation(*m_impl);
    auto& slot = m_impl->slots[entity.index];
    if (!slot.pending) { m_impl->pending.push_back(entity.index); slot.pending = true; }
    return Status::Ok();
}

Status World::CommitDestructions() {
    ALICE_TRY(m_impl->CanWrite());
    ALICE_PROFILE_ZONE("World.CommitDestructions");
    ecs::Mutation mutation(*m_impl);
    for (const u32 index : m_impl->pending) {
        for (auto& [type, pool] : m_impl->pools) { (void)type; pool->Erase(index); }
        auto& slot = m_impl->slots[index]; slot.alive = false; slot.pending = false;
        // Retire exhausted slots instead of allowing a stale generation to become valid again.
        if (slot.generation != std::numeric_limits<u32>::max()) {
            ++slot.generation; m_impl->freeSlots.push_back(index);
        }
    }
    m_impl->pending.clear();
    return Status::Ok();
}

Status World::SetComponent(EntityId entity, ComponentId component, const doc::Value& document,
                           const SourceLocation& source) {
    ALICE_TRY(m_impl->CanWrite(source)); ALICE_TRY(m_impl->CheckEntity(entity, source));
    const auto descriptor = m_impl->Describe(component, source); if (!descriptor) return descriptor.Error();
    ecs::Mutation mutation(*m_impl);
    DiagnosticBag diagnostics;
    ALICE_TRY(m_impl->registry->Validate(component, document, diagnostics, source));
    ecs::NativeValue temporary(*descriptor.Value());
    if (!temporary.Data()) return ecs::Error("runtime.storage.allocation_failed", "cannot allocate a temporary component",
        "free memory or reduce component size before retrying", source);
    const auto decoded = descriptor.Value()->decode(document, temporary.Data(), source);
    if (!decoded) return ecs::Located(decoded.Error(), source, document.mark, "components." + descriptor.Value()->name);
    auto* pool = m_impl->Pool(component);
    if (!pool) {
        auto created = std::make_unique<ecs::NativePool>(*descriptor.Value());
        pool = created.get(); m_impl->pools.emplace(component.index, std::move(created));
    }
    const auto stored = pool->Set(entity.index, temporary.Data());
    if (!stored) return ecs::Located(stored.Error(), source, document.mark);
    return Status::Ok();
}

Status World::RemoveComponent(EntityId entity, ComponentId component, const SourceLocation& source) {
    ALICE_TRY(m_impl->CanWrite(source)); ALICE_TRY(m_impl->CheckEntity(entity, source));
    const auto descriptor = m_impl->Describe(component, source); if (!descriptor) return descriptor.Error();
    auto* pool = m_impl->Pool(component);
    if (!pool || !pool->Find(entity.index)) return m_impl->Missing(entity, component, source);
    ecs::Mutation mutation(*m_impl); pool->Erase(entity.index);
    return Status::Ok();
}

Result<doc::Value> World::EncodeComponent(EntityId entity, ComponentId component, const SourceLocation& source) const {
    auto lease = AcquireRead(); if (!lease) return ecs::Located(lease.Error(), source);
    ALICE_TRY(m_impl->CheckEntity(entity, source));
    const auto descriptor = m_impl->Describe(component, source); if (!descriptor) return descriptor.Error();
    auto* pool = m_impl->Pool(component);
    const void* value = pool ? pool->Find(entity.index) : nullptr;
    if (!value) return m_impl->Missing(entity, component, source);
    auto encoded = descriptor.Value()->encode(value, source);
    if (!encoded) return ecs::Located(encoded.Error(), source, {}, "components." + descriptor.Value()->name);
    return std::move(encoded.Value());
}

Status World::EditComponent(EntityId entity, ComponentId component, std::type_index nativeType,
                            void (*edit)(void*, void*) noexcept, void* context, const SourceLocation& source) {
    ALICE_TRY(m_impl->CanWrite(source)); ALICE_TRY(m_impl->CheckEntity(entity, source));
    const auto descriptor = m_impl->Describe(component, source); if (!descriptor) return descriptor.Error();
    if (nativeType != descriptor.Value()->nativeType) return ecs::Error("runtime.component.type_mismatch",
        "edit type differs from the registered component type", "use the native type recorded in ComponentDescriptor", source);
    if (!edit) return ecs::Error("runtime.component.invalid_edit", "edit callback is null", "provide a valid edit callback", source);
    auto* pool = m_impl->Pool(component);
    void* value = pool ? pool->Find(entity.index) : nullptr;
    if (!value) return m_impl->Missing(entity, component, source);
    ecs::Mutation mutation(*m_impl); edit(value, context);
    return Status::Ok();
}

bool WorldReadView::IsAlive(EntityId entity) const noexcept {
    ALICE_ASSERT(m_impl, "cannot use a moved-from read view"); return m_impl->state->Alive(entity);
}
Result<ComponentId> WorldReadView::FindComponent(std::string_view name) const {
    ALICE_ASSERT(m_impl, "cannot use a moved-from read view"); return m_impl->state->registry->Find(name);
}
Result<const void*> WorldReadView::GetComponent(EntityId entity, ComponentId component, std::type_index nativeType) const {
    ALICE_ASSERT(m_impl, "cannot use a moved-from read view");
    const auto& state = *m_impl->state;
    ALICE_TRY(state.CheckEntity(entity));
    const auto descriptor = state.Describe(component); if (!descriptor) return descriptor.Error();
    if (nativeType != descriptor.Value()->nativeType) return ecs::Error("runtime.component.type_mismatch",
        "read type differs from the registered component type", "use the native type recorded in ComponentDescriptor", {},
        Fmt("entities[{}:{}:{}].components[{}]", entity.world, entity.index, entity.generation, component.index));
    const auto* pool = state.Pool(component);
    const void* value = pool ? pool->Find(entity.index) : nullptr;
    if (!value) return state.Missing(entity, component);
    return value;
}

Result<std::vector<EntityId>> WorldReadView::Query(std::span<const ComponentId> required) const {
    ALICE_ASSERT(m_impl, "cannot use a moved-from read view");
    ALICE_PROFILE_ZONE("World.Query");
    const auto& state = *m_impl->state;
    std::vector<const ecs::NativePool*> pools;
    const ecs::NativePool* smallest = nullptr;
    bool absent = false;
    for (const auto component : required) {
        const auto descriptor = state.Describe(component); if (!descriptor) return descriptor.Error();
        const auto* pool = state.Pool(component);
        if (!pool) { absent = true; continue; }
        pools.push_back(pool);
        if (!smallest || pool->Entities().size() < smallest->Entities().size()) smallest = pool;
    }
    std::vector<EntityId> result;
    if (absent) return result;
    if (required.empty()) {
        for (usize i = 1; i < state.slots.size(); ++i) if (state.slots[i].alive)
            result.push_back({state.identity, static_cast<u32>(i), state.slots[i].generation});
    } else if (smallest) {
        result.reserve(smallest->Entities().size());
        for (u32 index : smallest->Entities()) {
            if (std::all_of(pools.begin(), pools.end(), [index](const auto* pool) { return pool->Find(index) != nullptr; }))
                result.push_back({state.identity, index, state.slots[index].generation});
        }
        std::sort(result.begin(), result.end(), [](EntityId a, EntityId b) { return a.index < b.index; });
    }
    return result;
}
} // namespace alice::runtime
