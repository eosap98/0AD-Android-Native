// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <chrono>
#include <cmath>

namespace AndroidPerformance
{
// Fractional, monotonic deadlines avoid SDL_Delay's integer-millisecond drift.
// A missed budget starts a fresh interval instead of scheduling catch-up frames.
class FramePacer
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    TimePoint Schedule(TimePoint now, double fps, bool vsync = false, double refreshRate = 0)
    {
        // Swap already paces a 60 FPS game on a 60/59.94 Hz display. A second
        // timer can sleep past the next vblank and turn an achievable 60 into 30.
        const bool pacedByDisplay = vsync && std::isfinite(refreshRate) && refreshRate >= 20.0 &&
            fps >= refreshRate - 0.5;
        if (!std::isfinite(fps) || fps < 20.0 || fps >= 360.0 || pacedByDisplay)
        {
            m_Fps = 0;
            return now;
        }
        if (fps != m_Fps || now >= m_Next)
            m_Next = now;
        m_Fps = fps;
        const TimePoint deadline = m_Next;
        m_Next += std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / fps));
        return deadline;
    }

private:
    TimePoint m_Next{};
    double m_Fps = 0;
};
}
