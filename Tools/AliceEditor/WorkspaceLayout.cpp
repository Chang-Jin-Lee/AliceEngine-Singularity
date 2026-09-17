// SPDX-License-Identifier: MIT
// Clamp every divider independently while retaining useful sizes when the window grows.
#include "AliceEditor/WorkspaceLayout.h"
#include <algorithm>
#include <cmath>
namespace alice::editor {
WorkspaceGeometry WorkspaceLayout::Compute(int width, int height) const {
    width = std::max(1100, width); height = std::max(760, height);
    WorkspaceGeometry g{};
    g.editor = std::clamp(static_cast<int>(width * m_editor), 760, width - 280);
    g.left = std::clamp(static_cast<int>(g.editor * m_left), 160, g.editor - 500);
    g.right = std::clamp(g.editor - static_cast<int>(g.editor * m_inspector), g.left + 250, g.editor - 240);
    g.bottom = std::clamp(static_cast<int>(height * m_bottom), 630, height - 100);
    g.projectTop = std::clamp(145 + static_cast<int>((g.bottom - 145) * m_project), 280, g.bottom - 130);
    return g;
}
Split WorkspaceLayout::Hit(int x, int y, int width, int height) const {
    const auto g = Compute(width, height);
    if (y < 102 || y > height - 26) return Split::None;
    if (std::abs(x - g.editor) <= 4) return Split::Terminal;
    if (x < 8 || x >= g.editor - 5) return Split::None;
    if (std::abs(y - g.bottom) <= 4) return Split::Console;
    if (y >= g.bottom) return Split::None;
    if (std::abs(x - g.left) <= 4) return Split::Hierarchy;
    if (std::abs(x - g.right) <= 4) return Split::Inspector;
    if (x < g.left && std::abs(y - (g.projectTop - 30)) <= 4) return Split::Project;
    return Split::None;
}
void WorkspaceLayout::Drag(Split split, int x, int y, int width, int height) {
    const auto g = Compute(width, height);
    switch (split) {
    case Split::Terminal: m_editor = static_cast<double>(std::clamp(x, 760, std::max(820, width - 280))) / std::max(1100, width); break;
    case Split::Hierarchy: m_left = static_cast<double>(std::clamp(x, 160, g.right - 250)) / g.editor; break;
    case Split::Inspector: m_inspector = static_cast<double>(g.editor - std::clamp(x, g.left + 250, g.editor - 240)) / g.editor; break;
    case Split::Console: m_bottom = static_cast<double>(std::clamp(y, 630, std::max(660, height - 100))) / std::max(760, height); break;
    case Split::Project: m_project = static_cast<double>(std::clamp(y + 30, 280, g.bottom - 130) - 145) / (g.bottom - 145); break;
    case Split::None: break;
    }
}
}
