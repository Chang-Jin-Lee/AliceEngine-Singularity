// SPDX-License-Identifier: MIT
// Extreme drags and window resize must preserve accessible inspector and terminal controls.
#include "TestFramework.h"
#include "AliceEditor/WorkspaceLayout.h"
using namespace alice;
ALICE_TEST(EditorLayout, EveryDividerMovesAndHasAHitTarget) {
    editor::WorkspaceLayout layout;
    for (const auto split : {editor::Split::Terminal, editor::Split::Hierarchy, editor::Split::Inspector, editor::Split::Console, editor::Split::Project}) {
        const auto before = layout.Compute(1600, 1000);
        int x = split == editor::Split::Terminal ? before.editor : split == editor::Split::Hierarchy ? before.left : before.right;
        int y = split == editor::Split::Console ? before.bottom : split == editor::Split::Project ? before.projectTop - 30 : 300;
        if (split == editor::Split::Console || split == editor::Split::Project) x = 80;
        ALICE_CHECK(layout.Hit(x, y, 1600, 1000) == split);
        layout.Drag(split, x + 20, y + 20, 1600, 1000);
        ALICE_CHECK(layout.Hit(x + (y == 300 ? 20 : 0), y + (y == 300 ? 0 : 20), 1600, 1000) == split);
    }
}
ALICE_TEST(EditorLayout, ExtremeDragsAndResizeKeepPanelsUsable) {
    editor::WorkspaceLayout layout;
    for (int coordinate : {-10000, 10000}) for (const auto split : {editor::Split::Terminal, editor::Split::Hierarchy, editor::Split::Inspector, editor::Split::Console, editor::Split::Project}) {
        layout.Drag(split, coordinate, coordinate, 1600, 1000);
        for (const auto& [width, height] : {std::pair{1100, 760}, {1600, 1000}, {2400, 1400}}) {
            const auto g = layout.Compute(width, height);
            ALICE_CHECK(g.left >= 160); ALICE_CHECK(g.right - g.left >= 250);
            ALICE_CHECK(g.editor - g.right >= 240); ALICE_CHECK(width - g.editor >= 280);
            ALICE_CHECK(g.bottom >= 630); ALICE_CHECK(height - g.bottom >= 100);
            ALICE_CHECK(g.projectTop >= 280); ALICE_CHECK(g.bottom - g.projectTop >= 130);
        }
    }
}
