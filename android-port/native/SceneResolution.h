// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <cstdint>

namespace AndroidPerformance
{
struct Extent
{
    std::uint32_t width = 0, height = 0;
    bool operator==(const Extent&) const = default;
};

// This is an offscreen 3D extent. It must never be used to size the window,
// Android SurfaceView, GUI, camera viewport used for picking, or touch input.
inline Extent SceneExtent(Extent display, int shortEdge)
{
    const std::uint32_t edge = std::min(display.width, display.height);
    if (!edge || shortEdge <= 0 || static_cast<std::uint32_t>(shortEdge) >= edge)
        return display;
    const double scale = static_cast<double>(std::max(240, shortEdge)) / edge;
    if (scale >= 1.0)
        return display;
    return {std::max(2u, static_cast<std::uint32_t>(display.width * scale / 2) * 2),
        std::max(2u, static_cast<std::uint32_t>(display.height * scale / 2) * 2)};
}
}
