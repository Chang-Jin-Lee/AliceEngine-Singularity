// SPDX-License-Identifier: MIT
// Decode into separate storage so rejection cannot partially overwrite a live component.
#include "Runtime/ECS/NativePool.h"
#include "Runtime/ECS/Support.h"
#include <algorithm>
#include <new>

namespace alice::runtime::ecs {
namespace {
std::align_val_t Alignment(const ComponentDescriptor& d) {
    return static_cast<std::align_val_t>(std::max(d.alignment, alignof(std::max_align_t)));
}
}

NativeValue::NativeValue(const ComponentDescriptor& descriptor) : m_descriptor(descriptor) {
    m_data = ::operator new(descriptor.size, Alignment(descriptor), std::nothrow);
    if (m_data) m_descriptor.construct(m_data);
}
NativeValue::~NativeValue() {
    if (m_data) { m_descriptor.destroy(m_data); ::operator delete(m_data, Alignment(m_descriptor)); }
}

NativePool::NativePool(ComponentDescriptor descriptor) : m_descriptor(std::move(descriptor)) {}
NativePool::~NativePool() {
    for (usize i = 0; i < m_entities.size(); ++i) m_descriptor.destroy(m_data + i * m_descriptor.size);
    ::operator delete(m_data, Alignment(m_descriptor));
}

const void* NativePool::Find(u32 entity) const noexcept {
    if (entity >= m_sparse.size() || m_sparse[entity] == 0) return nullptr;
    return m_data + (m_sparse[entity] - 1) * m_descriptor.size;
}
void* NativePool::Find(u32 entity) noexcept { return const_cast<void*>(std::as_const(*this).Find(entity)); }

Status NativePool::Grow() {
    const usize limit = std::numeric_limits<usize>::max();
    if (m_capacity > limit / 2) return Error("runtime.storage.capacity", "native pool capacity overflow",
        "reduce the number of components in this World");
    const usize capacity = m_capacity ? m_capacity * 2 : 8;
    if (capacity > limit / m_descriptor.size) return Error("runtime.storage.capacity", "native pool byte size overflow",
        "reduce the component size or count");
    auto* data = static_cast<std::byte*>(::operator new(capacity * m_descriptor.size, Alignment(m_descriptor), std::nothrow));
    if (!data) return Error("runtime.storage.allocation_failed", "cannot allocate native component storage",
        "free memory or reduce the number of components");
    for (usize i = 0; i < m_entities.size(); ++i) {
        m_descriptor.moveConstruct(data + i * m_descriptor.size, m_data + i * m_descriptor.size);
        m_descriptor.destroy(m_data + i * m_descriptor.size);
    }
    ::operator delete(m_data, Alignment(m_descriptor));
    m_data = data; m_capacity = capacity;
    return Status::Ok();
}

Status NativePool::Set(u32 entity, void* value) {
    if (void* existing = Find(entity)) {
        m_descriptor.destroy(existing); m_descriptor.moveConstruct(existing, value);
        return Status::Ok();
    }
    if (m_entities.size() == m_capacity) { const auto result = Grow(); if (!result) return result; }
    if (m_sparse.size() <= entity) m_sparse.resize(static_cast<usize>(entity) + 1, 0);
    m_entities.push_back(entity);
    m_sparse[entity] = m_entities.size();
    m_descriptor.moveConstruct(m_data + (m_entities.size() - 1) * m_descriptor.size, value);
    return Status::Ok();
}

void NativePool::Erase(u32 entity) noexcept {
    if (!Find(entity)) return;
    const usize index = m_sparse[entity] - 1;
    const usize last = m_entities.size() - 1;
    m_descriptor.destroy(m_data + index * m_descriptor.size);
    if (index != last) {
        m_descriptor.moveConstruct(m_data + index * m_descriptor.size, m_data + last * m_descriptor.size);
        m_descriptor.destroy(m_data + last * m_descriptor.size);
        m_entities[index] = m_entities[last];
        m_sparse[m_entities[index]] = index + 1;
    }
    m_entities.pop_back(); m_sparse[entity] = 0;
}
} // namespace alice::runtime::ecs
