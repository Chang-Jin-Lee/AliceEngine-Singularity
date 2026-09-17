// Document policy is separate from the reusable, device-independent collision solver.
#pragma once
#include "Doc/Document.h"
#include "Runtime/Physics/Platformer.h"
#include <string>
#include <utility>
#include <vector>
namespace alice::editor {
class PlatformerSession {
public:
    Status Load(const doc::Document& scene, usize player, const doc::Document& input);
    Result<runtime::physics::Vec2> Tick(double dt, const std::vector<std::string>& heldKeys);
private:
    Status LoadDocument(const doc::Document& scene, usize player, const doc::Document& input);
    runtime::physics::StaticWorld m_world;
    runtime::physics::Box m_body;
    runtime::physics::Vec2 m_offset;
    runtime::SourceLocation m_source;
    double m_speed = 4, m_jumpSpeed = 7, m_gravity = -9.81, m_velocityY = 0, m_accumulator = 0;
    bool m_grounded = false, m_jumpHeld = false, m_jumpPending = false;
    std::vector<std::pair<std::string, std::string>> m_moveBindings;
    std::vector<std::string> m_jumpBindings;
};
}
