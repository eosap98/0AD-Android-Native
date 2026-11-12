// SPDX-License-Identifier: GPL-2.0-or-later
#include "FramePacer.h"
#include <iostream>
#include <limits>
#include <stdexcept>

using AndroidPerformance::FramePacer;
using namespace std::chrono;

static FramePacer::TimePoint At(double milliseconds)
{
    return FramePacer::TimePoint{} + duration_cast<FramePacer::Clock::duration>(duration<double, std::milli>(milliseconds));
}

static void Near(FramePacer::TimePoint actual, FramePacer::TimePoint expected)
{
    if (std::abs(duration<double, std::micro>(actual - expected).count()) > 4)
        throw std::runtime_error("Unexpected frame deadline");
}

int main()
{
    for (double fps : {20.0, 30.0, 60.0, 120.0})
    {
        FramePacer pacer;
        auto deadline = pacer.Schedule(At(0), fps);
        for (int i = 1; i <= 3000; ++i)
        {
            deadline = pacer.Schedule(deadline + milliseconds(2), fps);
            Near(deadline, At(i * 1000.0 / fps));
        }
        std::cout << "PASS fractional cadence at " << fps << " FPS for 3000 frames\n";
    }
    FramePacer slow;
    slow.Schedule(At(0), 30);
    Near(slow.Schedule(At(200), 30), At(200));
    Near(slow.Schedule(At(210), 30), At(200 + 1000.0 / 30));
    std::cout << "PASS slow frames do not create catch-up bursts\n";

    FramePacer resumed;
    resumed.Schedule(At(0), 30);
    Near(resumed.Schedule(At(3600000), 30), At(3600000));
    Near(resumed.Schedule(At(3600010), 30), At(3600000 + 1000.0 / 30));
    std::cout << "PASS resume resets an expired deadline\n";

    FramePacer changed;
    changed.Schedule(At(0), 30);
    Near(changed.Schedule(At(10), 60), At(10));
    Near(changed.Schedule(At(12), 60), At(10 + 1000.0 / 60));
    std::cout << "PASS changing the FPS limit resets the cadence\n";

    for (double fps : {0.0, -1.0, 19.0, 360.0, std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::quiet_NaN()})
    {
        FramePacer disabled;
        disabled.Schedule(At(0), 30);
        Near(disabled.Schedule(At(5), fps), At(5));
        Near(disabled.Schedule(At(10), 30), At(10));
    }
    std::cout << "PASS disabled and invalid limits cannot cause an excessive wait\n";
    for (double refreshRate : {59.0, 59.94, 60.0, 60.1})
    {
        FramePacer display;
        display.Schedule(At(0), 60);
        Near(display.Schedule(At(10), 60, true, refreshRate), At(10));
        Near(display.Schedule(At(20), 60, true, refreshRate), At(20));
    }
    std::cout << "PASS 60 FPS VSync does not add a second timer wait\n";

    FramePacer highRefresh;
    highRefresh.Schedule(At(0), 60, true, 120);
    Near(highRefresh.Schedule(At(5), 60, true, 120), At(1000.0 / 60));
    std::cout << "PASS a 120 Hz display still honours a lower 60 FPS budget\n";

    FramePacer noVsync;
    noVsync.Schedule(At(0), 60, false, 60);
    Near(noVsync.Schedule(At(5), 60, false, 60), At(1000.0 / 60));
    std::cout << "PASS disabling VSync restores software pacing\n";

    FramePacer unknownRefresh;
    unknownRefresh.Schedule(At(0), 60, true, 0);
    Near(unknownRefresh.Schedule(At(5), 60, true, 0), At(1000.0 / 60));
    std::cout << "PASS unknown display refresh keeps the software fallback\n";
    std::cout << "12 frame-pacing checks passed (scheduler tests, not measured device FPS).\n";
}
