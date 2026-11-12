// SPDX-License-Identifier: GPL-2.0-or-later
#include "precompiled.h"
#include "AndroidTouchInput.h"
#include "TouchGestures.h"

#include "graphics/CinemaManager.h"
#include "graphics/GameView.h"
#include "gui/CGUI.h"
#include "gui/GUIManager.h"
#include "lib/external_libraries/libsdl.h"
#include "lib/input.h"
#include "ps/Game.h"
#include "ps/Globals.h"
#include "ps/VideoMode.h"
#include "scriptinterface/Object.h"
#include "scriptinterface/ScriptInterface.h"
#include "scriptinterface/ScriptRequest.h"

#include <SDL.h>
#include <jni.h>
#include <deque>
#include <memory>
#include <mutex>

extern int g_xres, g_yres;

namespace AndroidTouch
{
namespace
{
std::mutex queueMutex;
std::deque<Sample> samples;
std::weak_ptr<CGUI> context, pressedGui;
CGame* contextGame = nullptr;
bool usingTouch = false, primaryDown = false, selecting = false;
CameraMotion motion;

std::shared_ptr<CGUI> ActiveGui()
{
    return g_GUI && g_GUI->GetPageCount() ? g_GUI->GetActiveGUI() : nullptr;
}

bool Session(const std::shared_ptr<CGUI>& gui)
{
    return gui && g_app_has_focus && g_Game && g_Game->IsGameStarted() &&
        g_Game->GetView() && !g_Game->GetView()->GetCinema()->IsEnabled() &&
        gui->ObjectExists("session");
}

Target TargetAt(Point point)
{
    auto gui = ActiveGui();
    if (!Session(gui) || gui->FindObjectAt({point.x / g_VideoMode.GetScale(),
            point.y / g_VideoMode.GetScale()}))
        return Target::Gui;
    const ScriptRequest rq(gui->GetScriptInterface());
    JS::RootedValue global(rq.cx, rq.globalValue());
    int state = -1;
    if (!Script::GetProperty(rq, global, "inputState", state))
        return Target::Gui;
    // INPUT_NORMAL, INPUT_BUILDING_PLACEMENT, INPUT_BUILDING_WALL_PATHING
    // from the bundled gui/session/input.js. Other explicit actions keep their
    // ordinary left-click path, including patrol targets and formation orders.
    if (state == 0)
        return Target::World;
    if (state == 3 || state == 9)
        return Target::Placement;
    return Target::Gui;
}

void MouseMove(Point point)
{
    SDL_Event_ event{};
    event.ev.type = SDL_MOUSEMOTION;
    event.ev.motion.which = SDL_TOUCH_MOUSEID;
    event.ev.motion.x = std::lround(point.x);
    event.ev.motion.y = std::lround(point.y);
    event.ev.motion.xrel = event.ev.motion.x - g_mouse_x;
    event.ev.motion.yrel = event.ev.motion.y - g_mouse_y;
    event.ev.motion.state = primaryDown ? SDL_BUTTON_LMASK : 0;
    in_dispatch_event(&event);
}

SDL_Event_ ButtonEvent(Point point, Uint8 button, bool down, Uint8 clicks = 1)
{
    SDL_Event_ event{};
    event.ev.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    event.ev.button.which = SDL_TOUCH_MOUSEID;
    event.ev.button.button = button;
    event.ev.button.state = down ? SDL_PRESSED : SDL_RELEASED;
    event.ev.button.clicks = clicks;
    event.ev.button.x = std::lround(point.x);
    event.ev.button.y = std::lround(point.y);
    return event;
}

void Button(Point point, Uint8 button, bool down, Uint8 clicks = 1)
{
    SDL_Event_ event = ButtonEvent(point, button, down, clicks);
    in_dispatch_event(&event);
}

// Cancel a partially drawn box without committing a selection or clearing the
// previously selected units. This uses the bundled session/input.js state.
constexpr const char* CancelSelectionScript = R"JS(
if (inputState === INPUT_SELECTING || inputState === INPUT_BANDBOXING)
{
    inputState = INPUT_NORMAL;
    clickedEntity = INVALID_ENTITY;
    Engine.GetGUIObjectByName("bandbox").hidden = true;
    g_Selection.setHighlightList([]);
}
else if (inputState === INPUT_BUILDING_CLICK || inputState === INPUT_BUILDING_DRAG ||
    inputState === INPUT_BUILDING_WALL_CLICK)
{
    inputState = INPUT_BUILDING_PLACEMENT;
}
)JS";

// clearSelection alone only cancels placement while retaining the builders.
// A two-finger tap always releases the entire selection as well.
constexpr const char* DeselectScript = R"JS(
clearSelection();
g_Selection.reset();
g_Selection.setHighlightList([]);
clickedEntity = INVALID_ENTITY;
)JS";

// A tap on our units/structures should select, not just highlight them or send
// a move/rally order. Keep specific context actions (gather, repair, attack,
// heal, trade, etc.) available when the selected units can perform them.
// With no actionable selection, a tap can also inspect another player's unit.
constexpr const char* SelectTapScript = R"JS(
(() => {
    if (inputState !== INPUT_NORMAL)
        return false;
    const entity = Engine.PickEntityAtPoint(mouseX, mouseY);
    const state = entity !== INVALID_ENTITY && GetEntityState(entity);
    if (!state)
        return false;
    const action = determineAction(mouseX, mouseY);
    if (!action || action.type === "none")
        return true;
    return state.player === g_ViewedPlayer &&
        (action.type === "move" || action.type === "set-rallypoint" || action.type === "unset-rallypoint");
})()
)JS";

bool TrySelectTap(Point point)
{
    if (TargetAt(point) != Target::World)
        return false;
    auto gui = ActiveGui();
    MouseMove(point);
    bool select = false;
    if (!gui->GetScriptInterface()->Eval(SelectTapScript, select) || !select)
        return false;
    // A real primary click updates EntitySelection, including its dirty flag
    // and change handlers. The regular session tick then refreshes the build
    // and action panels, just as with mouse selection or a held touch.
    Button(point, SDL_BUTTON_LEFT, true);
    Button(point, SDL_BUTTON_LEFT, false);
    return true;
}

void CancelPrimary()
{
    if (!primaryDown)
        return;
    auto gui = pressedGui.lock();
    if (gui && gui->ObjectExists("session") && g_Game == contextGame && g_Game && g_Game->IsGameStarted())
        gui->GetScriptInterface()->Eval(CancelSelectionScript);
    // Release outside every widget, directly on the captured page. Sending a
    // motion first could drag a slider to its end; dispatching on the new page
    // after navigation could activate an unrelated control.
    SDL_Event_ event = ButtonEvent({-10000, -10000}, SDL_BUTTON_LEFT, false);
    GlobalsInputHandler(&event);
    if (gui)
        gui->HandleEvent(&event);
    primaryDown = selecting = false;
    pressedGui.reset();
}

void Dispatch(const Output& output)
{
    auto gui = ActiveGui();
    switch (output.kind)
    {
    case Kind::GuiDown:
    case Kind::SelectBegin:
        if (!gui || (output.kind == Kind::SelectBegin && TargetAt(output.to) != Target::World))
            return;
        pressedGui = gui;
        selecting = output.kind == Kind::SelectBegin;
        MouseMove(output.to);
        primaryDown = true;
        Button(output.to, SDL_BUTTON_LEFT, true);
        break;
    case Kind::GuiMove:
    case Kind::SelectMove:
        if (!primaryDown || pressedGui.lock() != gui)
            return;
        MouseMove(output.to);
        // The first crossing starts INPUT_BANDBOXING; the next updates the box.
        if (selecting)
            MouseMove(output.to);
        break;
    case Kind::GuiUp:
    case Kind::SelectEnd:
        if (!primaryDown || pressedGui.lock() != gui)
            return;
        MouseMove(output.to);
        if (selecting)
            MouseMove(output.to);
        Button(output.to, SDL_BUTTON_LEFT, false);
        primaryDown = selecting = false;
        pressedGui.reset();
        break;
    case Kind::CancelGui:
    case Kind::CancelSelection:
        CancelPrimary();
        break;
    case Kind::Command:
        if (TargetAt(output.to) == Target::World)
        {
            MouseMove(output.to);
            Button(output.to, SDL_BUTTON_RIGHT, true);
            Button(output.to, SDL_BUTTON_RIGHT, false);
        }
        break;
    case Kind::SelectSimilar:
        if (TargetAt(output.to) == Target::World)
        {
            MouseMove(output.to);
            // The first down picks this unit, replacing any stale clickedEntity
            // from a previous selection. The double-click release uses the
            // engine's selection-group/rank/ownership rules for similar units.
            Button(output.to, SDL_BUTTON_LEFT, true);
            Button(output.to, SDL_BUTTON_LEFT, false, 2);
        }
        break;
    case Kind::Deselect:
        if (Session(gui))
            gui->GetScriptInterface()->Eval(DeselectScript);
        break;
    case Kind::PlacementPreview:
        if (TargetAt(output.to) == Target::Placement)
            MouseMove(output.to);
        break;
    case Kind::Place:
        if (TargetAt(output.to) == Target::Placement)
        {
            MouseMove(output.to);
            // Preview evaluation can cancel placement (e.g. insufficient
            // resources). Do not turn that cancellation into a selection click.
            if (TargetAt(output.to) == Target::Placement)
            {
                Button(output.to, SDL_BUTTON_LEFT, true);
                Button(output.to, SDL_BUTTON_LEFT, false);
            }
        }
        break;
    case Kind::Pan:
        if (Session(gui))
        {
            motion.x += output.to.x - output.from.x;
            motion.y += output.to.y - output.from.y;
        }
        break;
    case Kind::Transform:
        if (Session(gui))
        {
            motion.rotation += output.rotation;
            motion.zoom += output.zoom;
        }
        break;
    }
}

Gestures gestures(Dispatch, TrySelectTap);

void ClearQueue()
{
    std::lock_guard lock(queueMutex);
    samples.clear();
}
}

void Enqueue(Sample sample)
{
    std::lock_guard lock(queueMutex);
    if (sample.action == Action::Cancel || samples.size() >= 512)
    {
        samples.clear();
        sample = {}; // cancellation takes precedence over any delayed tap
    }
    samples.push_back(sample);
}

void Frame()
{
    motion = {};
    auto gui = ActiveGui();
    if (gui != context.lock() || contextGame != g_Game || !g_app_has_focus)
    {
        gestures.Cancel();
        CancelPrimary();
        ClearQueue();
        context = gui;
        contextGame = g_Game;
        return;
    }
    if (!gui || g_xres <= 0 || g_yres <= 0)
        return;
    std::deque<Sample> pending;
    {
        std::lock_guard lock(queueMutex);
        pending.swap(samples);
    }
    for (Sample& sample : pending)
    {
        if (sample.action != Action::Cancel)
            usingTouch = true;
        sample.density *= g_xres;
        for (Finger& finger : sample.fingers)
        {
            finger.point.x *= g_xres;
            finger.point.y *= g_yres;
        }
        Target target = Target::Gui;
        if (sample.action == Action::Down)
            target = TargetAt(sample.fingers[0].point);
        else if (sample.action == Action::PointerDown && sample.count == 2)
        {
            target = TargetAt(sample.fingers[0].point);
            if (target != TargetAt(sample.fingers[1].point))
                target = Target::Gui;
        }
        gestures.Handle(sample, target);
        if (ActiveGui() != gui || contextGame != g_Game)
        {
            gestures.Cancel();
            CancelPrimary();
            ClearQueue();
            motion = {};
            return;
        }
    }
    gestures.Tick(SDL_GetTicks64());
}

void OtherInput(const SDL_Event_* event)
{
    const bool mouse =
        (event->ev.type == SDL_MOUSEMOTION && event->ev.motion.which != SDL_TOUCH_MOUSEID) ||
        ((event->ev.type == SDL_MOUSEBUTTONDOWN || event->ev.type == SDL_MOUSEBUTTONUP) &&
            event->ev.button.which != SDL_TOUCH_MOUSEID) ||
        (event->ev.type == SDL_MOUSEWHEEL && event->ev.wheel.which != SDL_TOUCH_MOUSEID);
    if (mouse || event->ev.type == SDL_KEYDOWN ||
        (event->ev.type == SDL_WINDOWEVENT && event->ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST))
    {
        gestures.Cancel();
        CancelPrimary();
        ClearQueue();
        motion = {};
        if (mouse)
        {
            usingTouch = false;
            // GlobalsInputHandler already saw this physical event before us.
            // Preserve its button state after releasing the touch-owned click.
            GlobalsInputHandler(event);
        }
    }
}

CameraMotion ConsumeCameraMotion()
{
    CameraMotion result = motion;
    motion = {};
    return result;
}

bool UsingTouch() { return usingTouch; }
}

extern "C" JNIEXPORT void JNICALL
Java_org_wildfiregames_zeroad_GameActivity_nativeTouchFrame(
    JNIEnv*, jclass, jint action, jint count, jint firstId, jint secondId,
    jfloat x, jfloat y, jfloat secondX, jfloat secondY, jfloat density, jlong ageMs)
{
    AndroidTouch::Sample sample;
    sample.action = static_cast<AndroidTouch::Action>(action);
    sample.count = count;
    sample.fingers[0] = {firstId, {x, y}};
    sample.fingers[1] = {secondId, {secondX, secondY}};
    sample.density = density;
    const std::uint64_t now = SDL_GetTicks64();
    sample.time = now - std::min(now, static_cast<std::uint64_t>(std::max<jlong>(0, ageMs)));
    AndroidTouch::Enqueue(sample);
}

extern "C" JNIEXPORT void JNICALL
Java_org_wildfiregames_zeroad_GameActivity_nativeCancelTouches(JNIEnv*, jclass)
{
    AndroidTouch::Enqueue({});
}
