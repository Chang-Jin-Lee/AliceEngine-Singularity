// SPDX-License-Identifier: MIT
// A read lease, rather than const alone, keeps conditions and render extraction on one world state.
#pragma once

#include "Runtime/ComponentRegistry.h"

#include <span>
#include <vector>

namespace alice::runtime {

class World;

class WorldReadView {
public:
    ~WorldReadView();
    WorldReadView(WorldReadView&&) noexcept;
    WorldReadView& operator=(WorldReadView&&) noexcept;
    WorldReadView(const WorldReadView&) = delete;
    WorldReadView& operator=(const WorldReadView&) = delete;

    bool IsAlive(EntityId entity) const noexcept;
    Result<ComponentId> FindComponent(std::string_view name) const;
    Result<const void*> GetComponent(EntityId entity, ComponentId component,
                                    std::type_index nativeType) const;
    template <typename T>
    Result<const T*> Get(EntityId entity, ComponentId component) const {
        auto result = GetComponent(entity, component, typeid(T));
        if (!result) return result.Error();
        return static_cast<const T*>(result.Value());
    }
    // AND query, ordered by (index, generation). Empty requirements return all live entities.
    // Returned IDs are owned; native component pointers are borrowed only for this lease's lifetime.
    Result<std::vector<EntityId>> Query(std::span<const ComponentId> required) const;

private:
    friend class World;
    struct Impl;
    explicit WorldReadView(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};

class World {
public:
    // Fails unless registry is frozen. Shared ownership keeps component codecs alive.
    static Result<std::unique_ptr<World>> Create(std::shared_ptr<const ComponentRegistry> registry);
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Leases keep backing storage alive even if the World facade is destroyed.
    // Single-threaded API: no concurrent calls. Active leases reject ALL mutations, not just writes.
    Result<WorldReadView> AcquireRead() const;
    Result<EntityId> CreateEntity(const SourceLocation& source = {});
    // Destruction is deferred; a queued entity stays readable until CommitDestructions succeeds.
    Status DestroyEntity(EntityId entity, const SourceLocation& source = {});
    Status CommitDestructions();

    // Cold document boundary: validate using the frozen schema registry, then decode atomically.
    Status SetComponent(EntityId entity, ComponentId component, const doc::Value& document,
                        const SourceLocation& source = {});
    Status RemoveComponent(EntityId entity, ComponentId component, const SourceLocation& source = {});
    Result<doc::Value> EncodeComponent(EntityId entity, ComponentId component,
                                     const SourceLocation& source = {}) const;

    // Hot native updates stay in a callback so writable pointers cannot outlive the edit.
    // Type is checked before invocation. Callback must not retain the pointer or re-enter World.
    Status EditComponent(EntityId entity, ComponentId component, std::type_index nativeType,
                         void (*edit)(void* object, void* context) noexcept, void* context,
                         const SourceLocation& source = {});

private:
    struct Impl;
    explicit World(std::shared_ptr<Impl> impl);
    std::shared_ptr<Impl> m_impl;
};

} // namespace alice::runtime
