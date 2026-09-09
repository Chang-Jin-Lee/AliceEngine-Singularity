// SPDX-License-Identifier: MIT
// Schema codecs keep document conversion outside contiguous native component storage.
#pragma once

#include "Runtime/RuntimeTypes.h"
#include "Foundation/Result.h"
#include "Doc/Value.h"

#include <memory>
#include <string_view>
#include <typeindex>

namespace alice::schema { class Registry; }

namespace alice::runtime {

struct ComponentDescriptor {
    std::string name;       // Actor component key, e.g. "transform".
    std::string schemaId;   // Full versioned ID, e.g. "alice/component/transform/1".
    std::type_index nativeType{typeid(void)};
    usize size = 0;
    usize alignment = 0;

    // Runtime supplies aligned raw storage. Move constructs dst; src remains alive for Destroy.
    void (*construct)(void* dst) noexcept = nullptr;
    void (*destroy)(void* object) noexcept = nullptr;
    void (*moveConstruct)(void* dst, void* src) noexcept = nullptr;

    // Decode writes a constructed temporary; Runtime publishes it only on success.
    // Neither callback may retain pointers/references to its arguments or throw exceptions.
    Status (*decode)(const doc::Value&, void* dst, const SourceLocation&) = nullptr;
    Result<doc::Value> (*encode)(const void* object, const SourceLocation&) = nullptr;
};

class ComponentRegistry {
public:
    ComponentRegistry();
    ~ComponentRegistry();
    ComponentRegistry(const ComponentRegistry&) = delete;
    ComponentRegistry& operator=(const ComponentRegistry&) = delete;

    // Copies descriptor strings. Duplicate key/schema/native type or invalid callbacks fail.
    Result<ComponentId> Register(ComponentDescriptor descriptor, const SourceLocation& source = {});
    // Validates schema IDs and stores schema ownership; no registration after a successful freeze.
    // Caller must not mutate the shared schema registry or its schema graph while retained here.
    Status Freeze(std::shared_ptr<const schema::Registry> schemas);
    bool IsFrozen() const noexcept;
    Result<ComponentId> Find(std::string_view name) const;
    // Pointer survives until registry destruction once frozen; Register may invalidate it before then.
    Result<const ComponentDescriptor*> Describe(ComponentId component) const;
    // Requires Freeze. Append all validation diagnostics without clearing the caller's bag.
    // Return the first error added by this call, or success regardless of earlier bag entries.
    Status Validate(ComponentId component, const doc::Value& document, DiagnosticBag& diagnostics,
                    const SourceLocation& source = {}) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace alice::runtime
