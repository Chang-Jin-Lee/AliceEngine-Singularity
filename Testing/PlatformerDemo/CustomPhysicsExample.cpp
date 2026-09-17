// SPDX-License-Identifier: MIT
// A user provider can change motion policy while retaining the engine's collision and diagnostics.
#include "Runtime/Physics/Platformer.h"
#include <memory>

namespace alice::examples {
using namespace runtime;
using namespace runtime::physics;
class SlowMotionSolver final : public IMotionSolver {
public:
    Status Build(std::span<const Box> terrain) { return m_world.Build(terrain); }
    Result<Motion> Move(const Box& box, Vec2 displacement,
                        const SourceLocation& source = {}) const override {
        // No hidden character state: saving the caller's box and velocity is sufficient.
        return m_world.Move(box, {displacement.x * 0.5, displacement.y * 0.5}, source);
    }
private:
    StaticWorld m_world;
};
Result<std::unique_ptr<IMotionSolver>> MakeSlowMotionSolver(std::span<const Box> terrain) {
    auto solver = std::make_unique<SlowMotionSolver>();
    auto status = solver->Build(terrain);
    if (!status) return status.Error();
    return std::unique_ptr<IMotionSolver>{std::move(solver)};
}
} // namespace alice::examples
