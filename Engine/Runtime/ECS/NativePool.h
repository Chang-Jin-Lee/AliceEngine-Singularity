// SPDX-License-Identifier: MIT
// Type-erased storage must preserve native alignment and destroy every moved-from object exactly once.
#pragma once
#include "Runtime/ComponentRegistry.h"

namespace alice::runtime::ecs {
class NativeValue {
public:
    explicit NativeValue(const ComponentDescriptor& descriptor);
    ~NativeValue();
    NativeValue(const NativeValue&) = delete;
    NativeValue& operator=(const NativeValue&) = delete;
    void* Data() const noexcept { return m_data; }
private:
    const ComponentDescriptor& m_descriptor;
    void* m_data = nullptr;
};

class NativePool {
public:
    explicit NativePool(ComponentDescriptor descriptor);
    ~NativePool();
    NativePool(const NativePool&) = delete;
    NativePool& operator=(const NativePool&) = delete;
    const void* Find(u32 entity) const noexcept;
    void* Find(u32 entity) noexcept;
    const std::vector<u32>& Entities() const noexcept { return m_entities; }
    Status Set(u32 entity, void* value);
    void Erase(u32 entity) noexcept;
private:
    Status Grow();
    ComponentDescriptor m_descriptor;
    std::byte* m_data = nullptr;
    usize m_capacity = 0;
    std::vector<u32> m_entities;
    std::vector<usize> m_sparse; // Dense index + 1; zero is absent.
};
} // namespace alice::runtime::ecs
