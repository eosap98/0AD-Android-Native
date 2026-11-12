// SPDX-License-Identifier: GPL-2.0-or-later
#include "TouchGestures.h"
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace AndroidTouch;

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void Near(float actual, float expected)
{
    Require(std::abs(actual - expected) < 0.002f, "Unexpected transform amount");
}

struct Fixture
{
    std::vector<Output> outputs;
    std::vector<Point> tapSelections;
    bool guiHeld = false, selectionHeld = false;
    bool tapSelectable = false;
    float density = 1;
    Gestures gestures{[this](const Output& output)
    {
        Require(std::isfinite(output.rotation) && std::isfinite(output.zoom) &&
            std::isfinite(output.from.x) && std::isfinite(output.to.y), "Non-finite output");
        if (output.kind == Kind::GuiDown || output.kind == Kind::SelectBegin)
        {
            Require(!guiHeld && !selectionHeld, "Overlapping pointer presses");
            guiHeld = output.kind == Kind::GuiDown;
            selectionHeld = output.kind == Kind::SelectBegin;
        }
        if (output.kind == Kind::GuiUp || output.kind == Kind::CancelGui)
        {
            Require(guiHeld, "GUI release without a press");
            guiHeld = false;
        }
        if (output.kind == Kind::SelectEnd || output.kind == Kind::CancelSelection)
        {
            Require(selectionHeld, "Selection release without a press");
            selectionHeld = false;
        }
        if (output.kind == Kind::Command || output.kind == Kind::SelectSimilar || output.kind == Kind::Deselect ||
            output.kind == Kind::Place || output.kind == Kind::PlacementPreview ||
            output.kind == Kind::Pan || output.kind == Kind::Transform)
            Require(!guiHeld && !selectionHeld, "World action during a held GUI/selection click");
        outputs.push_back(output);
    }, [this](Point point)
    {
        Require(!guiHeld && !selectionHeld, "Tap selection during a held pointer");
        if (tapSelectable) tapSelections.push_back(point);
        return tapSelectable;
    }};

    void One(Action action, std::uint64_t time, float x = 100, float y = 100, bool world = true, int id = 7)
    {
        Sample sample{action, 1, {{id, {x, y}}, {}}, density, time};
        gestures.Handle(sample, world ? Target::World : Target::Gui);
    }
    void Pair(Action action, std::uint64_t time, Point first = {0, 0}, Point second = {100, 0},
        bool world = true, bool reverse = false)
    {
        Sample sample{action, 2, {{7, first}, {31, second}}, density, time};
        if (reverse) std::swap(sample.fingers[0], sample.fingers[1]);
        gestures.Handle(sample, world ? Target::World : Target::Gui);
    }
    void Placement(Action action, std::uint64_t time, float x = 100, float y = 100)
    {
        gestures.Handle({action, 1, {{7, {x, y}}, {}}, density, time}, Target::Placement);
    }
    void PlacementTap(std::uint64_t time = 0, float x = 100)
    {
        Placement(Action::Down, time, x);
        Placement(Action::Up, time + 40, x);
    }
    void Tap(std::uint64_t time = 0, float x = 100, bool world = true)
    {
        One(Action::Down, time, x, 100, world);
        One(Action::Up, time + 40, x, 100, world);
    }
    void StartPair()
    {
        One(Action::Down, 0, 0, 0);
        Pair(Action::PointerDown, 20);
    }
    int Count(Kind kind) const
    {
        return std::count_if(outputs.begin(), outputs.end(), [kind](const Output& x) { return x.kind == kind; });
    }
    float Rotation() const
    {
        float result = 0;
        for (const auto& output : outputs) result += output.rotation;
        return result;
    }
    float Zoom() const
    {
        float result = 0;
        for (const auto& output : outputs) result += output.zoom;
        return result;
    }
    void NoCommands() const
    {
        Require(Count(Kind::Command) == 0 && Count(Kind::SelectSimilar) == 0 &&
            Count(Kind::Deselect) == 0 && Count(Kind::Place) == 0, "Gesture generated an accidental command");
    }
};

int main()
{
    unsigned passed = 0;
    const auto run = [&](const char* name, const std::function<void(Fixture&)>& test)
    {
        Fixture f;
        try { test(f); f.gestures.Cancel(); }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
            throw;
        }
        Require(!f.guiHeld && !f.selectionHeld, "Stuck pointer after cancellation");
        std::cout << "PASS " << name << '\n';
        ++passed;
    };
    run("single tap waits for double-tap window", [](Fixture& f) {
        f.Tap(); f.gestures.Tick(339); Require(f.outputs.empty(), "Premature command");
        f.gestures.Tick(340); Require(f.Count(Kind::Command) == 1, "Missing command");
        f.gestures.Tick(900); Require(f.Count(Kind::Command) == 1, "Duplicate command");
    });
    run("selectable unit is selected on first release without holding or waiting", [](Fixture& f) {
        f.tapSelectable = true;
        f.One(Action::Down, 0);
        Require(f.tapSelections.empty(), "Selected before release could become a camera gesture");
        f.One(Action::Up, 40);
        Require(f.tapSelections.size() == 1, "Single-tap selection was delayed");
        f.gestures.Tick(340); f.gestures.Tick(1000);
        Require(f.tapSelections.size() == 1, "Duplicate selection"); f.NoCommands();
    });
    run("second tap upgrades immediate selection to similar units without an order", [](Fixture& f) {
        f.tapSelectable = true; f.Tap(); f.Tap(150); f.gestures.Tick(1000);
        Require(f.tapSelections.size() == 1 && f.Count(Kind::SelectSimilar) == 1 &&
            !f.Count(Kind::Command), "Immediate selection broke double tap");
    });
    run("distant selectable taps switch selection without flushing a stale order", [](Fixture& f) {
        f.tapSelectable = true; f.Tap(); f.Tap(150, 200); f.gestures.Tick(1000);
        Require(f.tapSelections.size() == 2, "Second unit was not selected"); f.NoCommands();
    });
    run("ground tap after immediate selection sends only the ground order", [](Fixture& f) {
        f.tapSelectable = true; f.Tap();
        f.tapSelectable = false; f.Tap(150, 200); f.gestures.Tick(1000);
        Require(f.tapSelections.size() == 1 && f.Count(Kind::Command) == 1,
            "Selected tap leaked an order or ground order was lost");
        Near(f.outputs.back().to.x, 200);
    });
    run("HUD interaction after selection cannot flush an unwanted unit order", [](Fixture& f) {
        f.tapSelectable = true; f.Tap(); f.Tap(100, 200, false); f.gestures.Tick(1000);
        Require(f.tapSelections.size() == 1 && f.Count(Kind::GuiUp) == 1, "HUD tap lost"); f.NoCommands();
    });
    run("drag beginning over a selectable unit pans without selecting it", [](Fixture& f) {
        f.tapSelectable = true; f.One(Action::Down, 0); f.One(Action::Move, 80, 140);
        f.One(Action::Up, 140, 160); f.gestures.Tick(1000);
        Require(f.tapSelections.empty() && f.Count(Kind::Pan) == 2, "Camera drag selected a unit"); f.NoCommands();
    });
    run("two-finger tap over a selectable unit deselects without selecting first", [](Fixture& f) {
        f.tapSelectable = true; f.StartPair(); f.Pair(Action::PointerUp, 60); f.One(Action::Up, 90, 0, 0);
        Require(f.tapSelections.empty() && f.Count(Kind::Deselect) == 1 &&
            !f.Count(Kind::Command), "Multi-touch committed the first finger as a selection");
    });
    run("a long hold on a selectable unit uses the held selection exactly once", [](Fixture& f) {
        f.tapSelectable = true; f.One(Action::Down, 0); f.One(Action::Up, 500); f.gestures.Tick(1000);
        Require(f.tapSelections.empty() && f.Count(Kind::SelectEnd) == 1, "Hold also selected as a tap"); f.NoCommands();
    });
    run("placement previews do not invoke immediate unit selection", [](Fixture& f) {
        f.tapSelectable = true; f.PlacementTap(); f.gestures.Tick(1000);
        Require(f.tapSelections.empty() && f.Count(Kind::PlacementPreview) == 1,
            "Preview became a selection"); f.NoCommands();
    });
    run("double tap selects similar units without sending the first command", [](Fixture& f) {
        f.Tap(); f.Tap(150); f.gestures.Tick(1000);
        Require(f.Count(Kind::SelectSimilar) == 1 && !f.Count(Kind::Deselect) && !f.Count(Kind::Command), "Wrong double tap action");
    });
    run("double tap accepts small finger drift", [](Fixture& f) {
        f.Tap(); f.Tap(200, 118); f.gestures.Tick(900);
        Require(f.Count(Kind::SelectSimilar) == 1 && f.Count(Kind::Command) == 0, "Drift rejected");
    });
    run("distant rapid taps remain separate commands", [](Fixture& f) {
        f.Tap(); f.Tap(150, 180); f.gestures.Tick(1000);
        Require(f.Count(Kind::Command) == 2 && f.Count(Kind::SelectSimilar) == 0, "Merged distant taps");
    });
    run("slow repeated tap is not a double tap", [](Fixture& f) {
        f.Tap(); f.Tap(500); f.gestures.Tick(1000);
        Require(f.Count(Kind::Command) == 2, "Lost slow tap");
    });
    run("drag pans and cannot turn into a long press", [](Fixture& f) {
        f.One(Action::Down, 0); f.One(Action::Move, 80, 140, 160);
        f.gestures.Tick(1000); f.One(Action::Up, 1100, 170, 190);
        Require(f.Count(Kind::Pan) == 2 && !f.Count(Kind::SelectBegin), "Wrong drag behavior"); f.NoCommands();
    });
    run("movement first reported on release still pans", [](Fixture& f) {
        f.One(Action::Down, 0); f.One(Action::Up, 100, 150); f.gestures.Tick(1000);
        Require(f.Count(Kind::Pan) == 1, "Release displacement lost"); f.NoCommands();
    });
    run("small tap jitter does not pan", [](Fixture& f) {
        f.One(Action::Down, 0); f.One(Action::Move, 30, 104, 101); f.One(Action::Up, 80, 103, 102);
        f.gestures.Tick(400); Require(f.Count(Kind::Command) == 1 && !f.Count(Kind::Pan), "Tap jitter misclassified");
    });
    run("hold then drag selects a box", [](Fixture& f) {
        f.One(Action::Down, 0); f.gestures.Tick(419); Require(f.outputs.empty(), "Early hold");
        f.gestures.Tick(420); f.One(Action::Move, 470, 200, 230); f.One(Action::Up, 540, 220, 250);
        Require(f.Count(Kind::SelectBegin) == 1 && f.Count(Kind::SelectEnd) == 1 && !f.Count(Kind::Pan), "Missing box"); f.NoCommands();
    });
    run("stationary hold selects a single unit", [](Fixture& f) {
        f.One(Action::Down, 0); f.One(Action::Up, 500);
        Require(f.Count(Kind::SelectBegin) == 1 && f.Count(Kind::SelectEnd) == 1, "Missing held selection"); f.NoCommands();
    });
    run("second tap becoming a drag cancels group selection", [](Fixture& f) {
        f.Tap(); f.One(Action::Down, 100); f.One(Action::Move, 180, 140); f.One(Action::Up, 220, 140);
        f.gestures.Tick(1000); Require(f.Count(Kind::Pan) == 1, "Missing second drag"); f.NoCommands();
    });
    run("second tap becoming a hold selects instead", [](Fixture& f) {
        f.Tap(); f.One(Action::Down, 100); f.One(Action::Up, 600);
        Require(f.Count(Kind::SelectEnd) == 1, "Missing second hold"); f.NoCommands();
    });
    run("menu tap uses immediate primary press and release", [](Fixture& f) {
        f.Tap(0, 100, false); Require(f.Count(Kind::GuiDown) == 1 && f.Count(Kind::GuiUp) == 1, "Menu delayed"); f.NoCommands();
    });
    run("menu double tap remains normal GUI input", [](Fixture& f) {
        f.Tap(0, 100, false); f.Tap(100, 100, false);
        Require(f.Count(Kind::GuiUp) == 2, "GUI tap lost"); f.NoCommands();
    });
    run("menu slider preserves primary drag", [](Fixture& f) {
        f.One(Action::Down, 0, 100, 100, false); f.One(Action::Move, 600, 220, 100, false);
        f.One(Action::Up, 700, 220, 100, false);
        Require(f.Count(Kind::GuiMove) == 1 && !f.Count(Kind::Pan) && !f.Count(Kind::SelectBegin), "GUI drag intercepted"); f.NoCommands();
    });
    run("menu touch cancels a pending world command", [](Fixture& f) {
        f.Tap(); f.Tap(100, 100, false); f.gestures.Tick(1000); f.NoCommands();
    });
    run("pinch zooms without issuing unit orders", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::Move, 80, {0, 0}, {150, 0}); Near(f.Zoom(), std::log(1.5f)); Near(f.Rotation(), 0); f.NoCommands();
    });
    run("inward pinch zooms out", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::Move, 80, {0, 0}, {70, 0}); Near(f.Zoom(), std::log(0.7f)); f.NoCommands();
    });
    run("clockwise twist", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::Move, 80, {0, 0}, {86.60254f, 50}); Near(f.Rotation(), 0.5235988f); Near(f.Zoom(), 0); f.NoCommands();
    });
    run("counterclockwise twist", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::Move, 80, {0, 0}, {86.60254f, -50}); Near(f.Rotation(), -0.5235988f); Near(f.Zoom(), 0);
    });
    run("twist and pinch in the same frame", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::Move, 80, {0, 0}, {129.90381f, 75}); Near(f.Rotation(), 0.5235988f); Near(f.Zoom(), std::log(1.5f));
    });
    run("two full turns cross the angle boundary smoothly", [](Fixture& f) {
        f.StartPair();
        for (int i = 1; i <= 72; ++i) {
            float angle = i * 0.174532925f;
            f.Pair(Action::Move, 20 + i * 16, {0, 0}, {100 * std::cos(angle), 100 * std::sin(angle)});
        }
        Near(f.Rotation(), 12.5663706f); Near(f.Zoom(), 0);
    });
    run("pointer-array reordering does not reverse rotation", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::Move, 80, {0, 0}, {86.60254f, 50}, true, true);
        Near(f.Rotation(), 0.5235988f);
    });
    run("two-finger translation does not cause spurious twist or pinch", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::Move, 80, {50, 60}, {150, 60}); Require(f.outputs.empty(), "Translation transformed");
    });
    run("two-finger tap deselects once after both fingers lift", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::PointerUp, 60);
        Require(f.outputs.empty(), "Deselect happened before final lift");
        f.One(Action::Up, 90, 0, 0); f.gestures.Tick(1000);
        Require(f.Count(Kind::Deselect) == 1 && !f.Count(Kind::Command) && !f.Count(Kind::SelectSimilar), "Missing two-finger deselect");
    });
    run("two-finger tap supports either release order and reordered IDs", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::PointerUp, 60, {}, {100, 0}, true, true);
        f.One(Action::Up, 100, 100, 0, true, 31);
        Require(f.Count(Kind::Deselect) == 1, "Second pointer release failed");
    });
    run("two-finger tap cancels a pending unit command", [](Fixture& f) {
        f.Tap(); f.One(Action::Down, 100);
        f.Pair(Action::PointerDown, 130, {100, 100}, {200, 100});
        f.Pair(Action::PointerUp, 160, {100, 100}, {200, 100});
        f.One(Action::Up, 190); f.gestures.Tick(1000);
        Require(f.Count(Kind::Deselect) == 1 && !f.Count(Kind::Command) && !f.Count(Kind::SelectSimilar), "Pending action leaked");
    });
    run("long two-finger hold does not deselect", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::PointerUp, 400); f.One(Action::Up, 440, 0, 0); f.NoCommands();
    });
    run("late second finger cannot turn a hold into a tap", [](Fixture& f) {
        f.One(Action::Down, 0, 0, 0); f.Pair(Action::PointerDown, 200);
        f.Pair(Action::PointerUp, 220); f.One(Action::Up, 260, 0, 0); f.NoCommands();
    });
    run("holding the last finger cancels a two-finger tap", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::PointerUp, 60); f.One(Action::Up, 400, 0, 0); f.NoCommands();
    });
    run("dragging the last finger cancels a two-finger tap", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::PointerUp, 60); f.One(Action::Move, 80, 30, 0);
        f.One(Action::Up, 100, 0, 0); f.NoCommands();
    });
    run("pair translation and return cannot become a tap", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::Move, 50, {20, 20}, {120, 20});
        f.Pair(Action::PointerUp, 80); f.One(Action::Up, 100, 0, 0); f.NoCommands();
    });
    run("release-only displacement cancels a two-finger tap", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::PointerUp, 60, {30, 0}, {130, 0});
        f.One(Action::Up, 90, 30, 0); f.NoCommands();
    });
    run("slight twist inside tap slop remains a two-finger tap", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::Move, 40, {}, {99.7564f, 6.9756f});
        f.Pair(Action::PointerUp, 60, {}, {99.7564f, 6.9756f}); f.One(Action::Up, 90, 0, 0);
        Require(f.Count(Kind::Deselect) == 1 && !f.Count(Kind::Transform), "Tap jitter stole the gesture");
    });
    run("a held pair can rotate precisely after the tap timeout", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::Move, 340, {}, {99.7564f, 6.9756f});
        f.Pair(Action::PointerUp, 360, {}, {99.7564f, 6.9756f}); f.One(Action::Up, 390, 0, 0);
        Require(f.Count(Kind::Transform) > 0, "Rotation not exercised"); f.NoCommands();
    });
    run("cancelling between finger releases does not deselect", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::PointerUp, 60); f.gestures.Cancel();
        f.One(Action::Up, 90, 0, 0); f.NoCommands();
    });
    run("a camera drag followed by a second finger cannot deselect", [](Fixture& f) {
        f.One(Action::Down, 0, 0, 0); f.One(Action::Move, 30, 20, 0);
        f.Pair(Action::PointerDown, 50, {20, 0}, {100, 0});
        f.Pair(Action::PointerUp, 80, {20, 0}, {100, 0}); f.One(Action::Up, 100, 20, 0); f.NoCommands();
    });
    run("remaining finger after pinch cannot pan or command", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::Move, 70, {0, 0}, {150, 0}); f.Pair(Action::PointerUp, 90);
        f.One(Action::Move, 200, 240); f.One(Action::Up, 250, 240); f.gestures.Tick(1000);
        Require(!f.Count(Kind::Pan), "Remaining finger panned"); f.NoCommands();
    });
    run("replacing a lifted finger drains the old gesture", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::PointerUp, 90); f.Pair(Action::PointerDown, 120);
        f.Pair(Action::Move, 180, {0, 0}, {0, 100}); f.Pair(Action::PointerUp, 190); f.One(Action::Up, 220);
        Require(f.outputs.empty(), "Replacement created a transform"); f.Tap(1000); f.gestures.Tick(1400);
        Require(f.Count(Kind::Command) == 1, "New independent gesture did not recover");
    });
    run("close fingers rebase without a half-turn jump", [](Fixture& f) {
        f.One(Action::Down, 0, 0, 0); f.Pair(Action::PointerDown, 20, {0, 0}, {10, 0});
        f.Pair(Action::Move, 40, {0, 0}, {-10, 0}); f.Pair(Action::Move, 60, {0, 0}, {-100, 0});
        Require(f.outputs.empty(), "Close pair jumped");
    });
    run("crossing pair with missing intermediate frames rebases", [](Fixture& f) {
        f.StartPair(); f.Pair(Action::Move, 70, {0, 0}, {-100, 0}); Require(f.outputs.empty(), "Half-turn jump");
    });
    run("small multi-touch jitter has no output", [](Fixture& f) {
        f.StartPair();
        for (int i = 0; i < 20; ++i) f.Pair(Action::Move, 40 + i * 16, {0, 0}, {100.0f + (i % 2), float(i % 2)});
        Require(f.outputs.empty(), "Jitter changed camera");
    });
    run("second finger over HUD cancels world gesture", [](Fixture& f) {
        f.One(Action::Down, 0); f.Pair(Action::PointerDown, 40, {100, 100}, {200, 100}, false);
        f.Pair(Action::Move, 80, {100, 100}, {300, 100}); Require(f.outputs.empty(), "HUD finger moved camera");
    });
    run("multi-touch on menu cancels the pressed widget", [](Fixture& f) {
        f.One(Action::Down, 0, 100, 100, false); f.Pair(Action::PointerDown, 60, {}, {100, 0}, false);
        Require(f.Count(Kind::CancelGui) == 1 && !f.Count(Kind::GuiUp), "Menu clicked on cancellation");
    });
    run("second finger cancels an in-progress box", [](Fixture& f) {
        f.One(Action::Down, 0); f.gestures.Tick(500); f.Pair(Action::PointerDown, 550);
        Require(f.Count(Kind::CancelSelection) == 1 && !f.Count(Kind::SelectEnd), "Box committed on multi-touch");
    });
    run("cancellation prevents a delayed command", [](Fixture& f) {
        f.Tap(); f.gestures.Cancel(); f.gestures.Tick(1000); f.NoCommands();
    });
    run("cancellation releases a box without selecting", [](Fixture& f) {
        f.One(Action::Down, 0); f.gestures.Tick(500); f.gestures.Cancel(); f.One(Action::Up, 600);
        Require(f.Count(Kind::CancelSelection) == 1 && !f.Count(Kind::SelectEnd), "Cancelled box selected");
    });
    run("cancellation cannot click a GUI control", [](Fixture& f) {
        f.One(Action::Down, 0, 100, 100, false); f.gestures.Cancel(); f.One(Action::Up, 100, 100, 100, false);
        Require(f.Count(Kind::CancelGui) == 1 && !f.Count(Kind::GuiUp), "Cancelled GUI clicked");
    });
    run("page or focus change discards pending input", [](Fixture& f) {
        f.Tap(); f.gestures.Cancel(); f.Tap(100, 100, false); f.gestures.Tick(1000); f.NoCommands();
    });
    run("new DOWN recovers from a missing old UP", [](Fixture& f) {
        f.One(Action::Down, 0, 100, 100, false); f.One(Action::Down, 40, 100, 100, false); f.One(Action::Up, 80, 100, 100, false);
        Require(f.Count(Kind::CancelGui) == 1 && f.Count(Kind::GuiUp) == 1, "Old pointer not released");
    });
    run("three fingers cancel and cannot issue a command", [](Fixture& f) {
        f.StartPair(); Sample sample{Action::PointerDown, 3, {{7, {}}, {31, {100, 0}}}, 1, 80};
        f.gestures.Handle(sample, Target::World); f.Pair(Action::Move, 100, {}, {200, 0}); f.One(Action::Up, 200);
        f.gestures.Tick(1000); Require(f.outputs.empty(), "Third finger generated input");
    });
    run("duplicate pointer IDs cancel", [](Fixture& f) {
        f.StartPair(); Sample sample{Action::Move, 2, {{7, {}}, {7, {100, 0}}}, 1, 80};
        f.gestures.Handle(sample, Target::World); Require(f.outputs.empty(), "Duplicate IDs generated input");
    });
    run("arbitrary sparse pointer ID is supported", [](Fixture& f) {
        f.One(Action::Down, 0, 100, 100, true, 100000); f.One(Action::Up, 40, 100, 100, true, 100000);
        f.gestures.Tick(400); Require(f.Count(Kind::Command) == 1, "Sparse ID failed");
    });
    run("non-finite coordinates cancel safely", [](Fixture& f) {
        f.One(Action::Down, 0); f.One(Action::Move, 40, std::numeric_limits<float>::quiet_NaN());
        f.One(Action::Up, 80); f.gestures.Tick(1000); Require(f.outputs.empty(), "NaN generated input");
    });
    run("backward timer tick cannot trigger a hold", [](Fixture& f) {
        f.One(Action::Down, 500); f.gestures.Tick(100); Require(f.outputs.empty(), "Timer underflow");
    });
    run("one build tap previews only even after the double-tap timeout", [](Fixture& f) {
        f.PlacementTap(); f.gestures.Tick(1000);
        Require(f.Count(Kind::PlacementPreview) > 0 && !f.Count(Kind::GuiDown), "Missing preview"); f.NoCommands();
    });
    run("two build taps place once and never send a unit command", [](Fixture& f) {
        f.PlacementTap(); f.PlacementTap(150); f.gestures.Tick(1000);
        Require(f.Count(Kind::Place) == 1 && !f.Count(Kind::GuiDown) && !f.Count(Kind::SelectSimilar) &&
            !f.Count(Kind::Command), "Double-tap did not confirm building");
    });
    run("build drag only moves the preview", [](Fixture& f) {
        f.Placement(Action::Down, 0); f.Placement(Action::Move, 100, 240);
        f.Placement(Action::Up, 150, 240); f.gestures.Tick(1000);
        Require(f.Count(Kind::PlacementPreview) == 2 && !f.Count(Kind::Pan) && !f.Count(Kind::SelectBegin), "Wrong preview drag"); f.NoCommands();
    });
    run("a long build hold cannot prime a placement confirmation", [](Fixture& f) {
        f.Placement(Action::Down, 0); f.gestures.Tick(500); f.Placement(Action::Up, 600);
        f.PlacementTap(700); f.gestures.Tick(1500); f.NoCommands();
    });
    run("dragging the second build tap cancels confirmation", [](Fixture& f) {
        f.PlacementTap(); f.Placement(Action::Down, 150); f.Placement(Action::Move, 190, 180);
        f.Placement(Action::Up, 240, 100); f.gestures.Tick(1000); f.NoCommands();
    });
    run("holding the second build tap cancels confirmation", [](Fixture& f) {
        f.PlacementTap(); f.Placement(Action::Down, 150); f.Placement(Action::Up, 650);
        f.gestures.Tick(1000); f.NoCommands();
    });
    run("distant or slow build taps never place", [](Fixture& f) {
        f.PlacementTap(); f.PlacementTap(150, 200); f.PlacementTap(700, 200);
        f.gestures.Tick(1500); f.NoCommands();
    });
    run("a world tap cannot confirm a newly entered build mode", [](Fixture& f) {
        f.Tap(); f.PlacementTap(150); f.gestures.Tick(1000); f.NoCommands();
    });
    run("a placement tap cannot become a group selection after leaving build mode", [](Fixture& f) {
        f.PlacementTap(); f.Tap(150); f.gestures.Tick(1000);
        Require(f.Count(Kind::Command) == 1 && !f.Count(Kind::SelectSimilar) && !f.Count(Kind::Place), "Cross-mode double tap");
    });
    run("GUI interaction and cancellation discard pending build confirmation", [](Fixture& f) {
        f.PlacementTap(); f.Tap(100, 100, false); f.PlacementTap(180);
        f.gestures.Cancel(); f.PlacementTap(280); f.gestures.Tick(1000); f.NoCommands();
    });
    run("two-finger tap in build mode deselects without placing", [](Fixture& f) {
        f.Placement(Action::Down, 0, 0, 0);
        f.gestures.Handle({Action::PointerDown, 2, {{7, {}}, {31, {100, 0}}}, 1, 20}, Target::Placement);
        f.Pair(Action::PointerUp, 60); f.One(Action::Up, 90, 0, 0);
        Require(f.Count(Kind::Deselect) == 1 && !f.Count(Kind::Place) && !f.Count(Kind::GuiDown), "Build gesture placed unexpectedly");
    });
    for (float density : {0.75f, 1.5f, 3.0f, 4.0f})
    {
        run(("density-scaled tap and drag at " + std::to_string(density)).c_str(), [density](Fixture& f) {
            f.density = density; f.One(Action::Down, 0); f.One(Action::Move, 30, 100 + 7 * density);
            Require(!f.Count(Kind::Pan), "Density made slop too small");
            f.One(Action::Move, 60, 100 + 10 * density); f.One(Action::Up, 90, 100 + 10 * density);
            Require(f.Count(Kind::Pan) == 1, "Density made slop too large"); f.NoCommands();
        });
        run(("two-finger tap tolerates jitter at " + std::to_string(density)).c_str(), [density](Fixture& f) {
            f.density = density;
            f.One(Action::Down, 0, 0, 0);
            f.Pair(Action::PointerDown, 20, {}, {100 * density, 0});
            f.Pair(Action::Move, 40, {}, {99.7564f * density, 6.9756f * density});
            f.Pair(Action::PointerUp, 60, {}, {99.7564f * density, 6.9756f * density});
            f.One(Action::Up, 90, density, density);
            Require(f.Count(Kind::Deselect) == 1 && !f.Count(Kind::Transform) &&
                !f.Count(Kind::Command), "Density caused tap jitter to rotate or command");
        });
    }
    run("10000 mixed events keep pointer ownership balanced", [](Fixture& f) {
        std::mt19937 random(0x0AD28002);
        std::uint64_t time = 0;
        const Action actions[]{Action::Down, Action::Move, Action::Up, Action::PointerDown, Action::PointerUp, Action::Cancel};
        for (int i = 0; i < 10000; ++i)
        {
            time += random() % 120;
            Sample sample{actions[random() % 6], int(random() % 4),
                {{7, {float(random() % 1200), float(random() % 700)}},
                 {31, {float(random() % 1200), float(random() % 700)}}}, 1, time};
            f.gestures.Handle(sample, static_cast<Target>(random() % 3));
            if (random() % 3 == 0) f.gestures.Tick(time + 20);
        }
        f.gestures.Cancel(); f.gestures.Tick(time + 1000);
    });
    std::cout << passed << " scenarios passed; 10000 mixed events; no Android emulator used.\n";
}
