// SPDX-License-Identifier: MIT
// Keeping motion separate from input and gravity lets applications supply their own physics policy.
#pragma once
#include "Foundation/Result.h"
#include "Runtime/RuntimeTypes.h"
#include <span>
#include <vector>

namespace alice::runtime::physics {
struct Vec2 { double x = 0; double y = 0; };
struct Box { Vec2 center; Vec2 half; };
// Shared by document adapters so a bad terrain box retains its own source location.
// Centers/half extents are bounded by 1e6 to keep the 1e-8 contact tolerance meaningful.
Status CheckBox(const Box& box, const SourceLocation& source = {});
struct Motion {
    Box box;
    bool blockedX = false, blockedY = false, grounded = false;
    usize candidates = 0, tests = 0;
};
// Providers own no character state. Position/velocity serialization belongs to the caller.
// Y points up; positive, finite half extents and finite displacements are required.
class IMotionSolver {
public:
    virtual ~IMotionSolver() = default;
    virtual Result<Motion> Move(const Box& box, Vec2 displacement,
                                const SourceLocation& source = {}) const = 0;
};
class StaticWorld final : public IMotionSolver {
public:
    // Exhaustive mode uses the same narrow phase for reproducible index comparisons.
    explicit StaticWorld(bool indexed = true) : m_indexed(indexed) {}
    // Failure preserves the previous terrain. The solver allocates terrain storage only here.
    Status Build(std::span<const Box> terrain, const SourceLocation& source = {});
    // Contact tolerance 1e-8 world units; 16 overlap corrections and 4 sweep iterations.
    // Zero displacement resolves spawns and refreshes grounding. Unsupported traps fail.
    // At the sweep limit unused displacement is discarded, never applied unchecked.
    Result<Motion> Move(const Box& box, Vec2 displacement,
                        const SourceLocation& source = {}) const override;
private:
    struct Node { Box bounds; usize begin = 0, end = 0, left = 0, right = 0; };
    usize BuildNode(usize begin, usize end);
    void Query(usize node, const Box& bounds, void (*visit)(usize, void*), void* context) const;
    void Visit(const Box& bounds, void (*visit)(usize, void*), void* context) const;
    std::vector<Box> m_boxes;
    std::vector<usize> m_order;
    std::vector<Node> m_nodes;
    bool m_indexed;
};
} // namespace alice::runtime::physics
