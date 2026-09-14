// SPDX-License-Identifier: MIT
// Keep runtime failures repairable and identities independent of allocator address reuse.
#pragma once
#include "Runtime/RuntimeTypes.h"
#include "Foundation/Result.h"
#include "Foundation/StringUtil.h"
#include <atomic>
#include <limits>

namespace alice::runtime::ecs {
inline u64 FreshIdentity() {
    static std::atomic<u64> next{1};
    u64 current = next.load(std::memory_order_relaxed);
    for (;;) {
        ALICE_ASSERT(current != std::numeric_limits<u64>::max(), "runtime identity space exhausted");
        if (next.compare_exchange_weak(current, current + 1, std::memory_order_relaxed)) return current;
    }
}

inline Diagnostic Located(Diagnostic error, const SourceLocation& source,
                          Mark fallback = {}, std::string fallbackPath = "runtime") {
    if (error.file.empty()) error.file = source.file;
    if (error.path.empty()) error.path = source.path.empty() ? std::move(fallbackPath) : source.path;
    if (!error.mark.Valid()) error.mark = fallback.Valid() ? fallback : source.mark;
    if (error.hint.empty()) error.hint = "check the component schema and codec input, then retry";
    if (error.code.empty()) error.code = "runtime.component.codec_failed";
    return error;
}

inline Diagnostic Error(std::string code, std::string message, std::string hint,
                        const SourceLocation& source = {}, std::string path = "runtime") {
    return Located(MakeError(std::move(code), std::move(message), std::move(hint)), source, {}, std::move(path));
}
} // namespace alice::runtime::ecs
