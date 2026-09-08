// SPDX-License-Identifier: MIT
// World/registry provenance prevents handles from silently naming another owner's data.
#pragma once

#include "Foundation/Diagnostic.h"

#include <compare>
#include <string>

namespace alice::runtime {

// Zero fields are invalid. Slots start at 1; exhausted generations are retired, never wrapped.
struct EntityId {
    u64 world = 0;
    u32 index = 0;
    u32 generation = 0;
    bool operator==(const EntityId&) const = default;
};

// Registry IDs are process-local, never serialized. Frozen registries can serve several worlds.
struct ComponentId {
    u64 registry = 0;
    u32 index = 0;
    bool operator==(const ComponentId&) const = default;
};

// Own strings so deferred commands can still report the originating document after unloading it.
struct SourceLocation {
    std::string file;
    std::string path;
    Mark mark;
};

} // namespace alice::runtime
