// SPDX-License-Identifier: MIT
// A bounded isometric projection makes scene placement visible before render backends exist.
#include "AliceEditor/SceneViewport.h"
#include <algorithm>
#include <cmath>

namespace alice::editor {
namespace {
POINT Project(RECT r, const Viewport& view, double x, double y, double z) {
    auto bounded = [](double value) {
        // Huge valid document numbers may overflow during projection; never cast NaN to an integer.
        return std::isnan(value) ? LONG{0} : static_cast<LONG>(std::clamp(value, -30000.0, 30000.0));
    };
    return {bounded((r.left + r.right) * 0.5 + view.pan.x + (x - z) * view.zoom),
            bounded((r.top + r.bottom) * 0.56 + view.pan.y + (x + z) * view.zoom * 0.42 - y * view.zoom)};
}
void Line(HDC dc, POINT a, POINT b) { MoveToEx(dc, a.x, a.y, nullptr); LineTo(dc, b.x, b.y); }
POINT Center(RECT r, const Viewport& view, const Actor& actor) { return Project(r, view, actor.position[0], actor.position[1], actor.position[2]); }
}
void Viewport::Paint(HDC dc, RECT bounds, const std::vector<Actor>& actors, int selected) const {
    HBRUSH background = CreateSolidBrush(RGB(25, 29, 36)); FillRect(dc, &bounds, background); DeleteObject(background);
    const int saved = SaveDC(dc); IntersectClipRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    HPEN grid = CreatePen(PS_SOLID, 1, RGB(46, 53, 62)); SelectObject(dc, grid);
    for (int i = -20; i <= 20; ++i) {
        Line(dc, Project(bounds, *this, i, 0, -20), Project(bounds, *this, i, 0, 20));
        Line(dc, Project(bounds, *this, -20, 0, i), Project(bounds, *this, 20, 0, i));
    }
    HPEN xAxis = CreatePen(PS_SOLID, 1, RGB(167, 83, 88)); SelectObject(dc, xAxis);
    Line(dc, Project(bounds, *this, -20, 0, 0), Project(bounds, *this, 20, 0, 0));
    HPEN zAxis = CreatePen(PS_SOLID, 1, RGB(76, 135, 170)); SelectObject(dc, zAxis);
    Line(dc, Project(bounds, *this, 0, 0, -20), Project(bounds, *this, 0, 0, 20));
    SetBkMode(dc, TRANSPARENT);
    for (usize index = 0; index < actors.size(); ++index) {
        const auto& actor = actors[index];
        const COLORREF color = static_cast<int>(index) == selected ? RGB(106, 224, 176) : RGB(157, 173, 196);
        HPEN pen = CreatePen(PS_SOLID, static_cast<int>(index) == selected ? 2 : 1, color); SelectObject(dc, pen);
        POINT corners[8];
        for (int i = 0; i < 8; ++i) {
            double v[3] = {(i & 1 ? .5 : -.5) * actor.scale[0], (i & 2 ? .5 : -.5) * actor.scale[1], (i & 4 ? .5 : -.5) * actor.scale[2]};
            for (int axis = 0; axis < 3; ++axis) {
                const int a = (axis + 1) % 3, b = (axis + 2) % 3;
                const double rad = actor.rotation[static_cast<usize>(axis)] * 0.0174532925199433;
                const double first = v[a] * std::cos(rad) - v[b] * std::sin(rad);
                v[b] = v[a] * std::sin(rad) + v[b] * std::cos(rad); v[a] = first;
            }
            corners[i] = Project(bounds, *this, v[0] + actor.position[0], v[1] + actor.position[1], v[2] + actor.position[2]);
        }
        for (int i = 0; i < 8; ++i) for (int bit : {1, 2, 4}) if (!(i & bit)) Line(dc, corners[i], corners[i | bit]);
        POINT p = Center(bounds, *this, actor);
        Line(dc, {p.x - 4, p.y}, {p.x + 4, p.y}); Line(dc, {p.x, p.y - 4}, {p.x, p.y + 4});
        const int length = MultiByteToWideChar(CP_UTF8, 0, actor.name.data(), static_cast<int>(actor.name.size()), nullptr, 0);
        std::wstring label(static_cast<usize>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, actor.name.data(), static_cast<int>(actor.name.size()), label.data(), length);
        SetTextColor(dc, color); TextOutW(dc, p.x + 9, p.y - 20, label.c_str(), length);
        SelectObject(dc, grid); DeleteObject(pen);
    }
    RestoreDC(dc, saved); DeleteObject(grid); DeleteObject(xAxis); DeleteObject(zAxis);
}
int Viewport::Hit(RECT bounds, POINT point, const std::vector<Actor>& actors) const {
    int best = -1; double nearest = 30.0 * 30.0;
    for (usize i = 0; i < actors.size(); ++i) {
        const POINT p = Center(bounds, *this, actors[i]);
        const double dx = point.x - p.x, dy = point.y - p.y, distance = dx * dx + dy * dy;
        if (distance < nearest) { nearest = distance; best = static_cast<int>(i); }
    }
    return best;
}
} // namespace alice::editor
