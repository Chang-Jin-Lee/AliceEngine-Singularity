// SPDX-License-Identifier: MIT
// Panel geometry is platform independent so dragging cannot silently hide controls.
#pragma once
namespace alice::editor {
enum class Split { None, Terminal, Hierarchy, Inspector, Console, Project };
struct WorkspaceGeometry {
    int editor, left, right, bottom, projectTop;
};
class WorkspaceLayout {
public:
    WorkspaceGeometry Compute(int width, int height) const;
    Split Hit(int x, int y, int width, int height) const;
    void Drag(Split split, int x, int y, int width, int height);
private:
    double m_editor = .74, m_left = .20, m_inspector = .25, m_bottom = .80, m_project = .50;
};
}
