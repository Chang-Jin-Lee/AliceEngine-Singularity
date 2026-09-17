// SPDX-License-Identifier: MIT
// Project document transforms without requiring unavailable mesh assets or a GPU backend.
#pragma once
#include "AliceEditor/DocumentModel.h"
#include <windows.h>
namespace alice::editor {
struct Viewport {
    double zoom = 24;
    POINT pan{};
    void Paint(HDC dc, RECT bounds, const std::vector<Actor>& actors, int selected) const;
    int Hit(RECT bounds, POINT point, const std::vector<Actor>& actors) const;
};
}
