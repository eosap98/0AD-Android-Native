// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <utility>

namespace AndroidTouch
{
struct Point { float x = 0, y = 0; };
struct Finger { int id = -1; Point point; };
// Android MotionEvent action values. Each sample contains both finger positions
// from the same event, so moving one finger cannot produce a partial transform.
enum class Action { Down = 0, Up = 1, Move = 2, Cancel = 3, PointerDown = 5, PointerUp = 6 };
struct Sample
{
    Action action = Action::Cancel;
    int count = 0;
    Finger fingers[2];
    float density = 1;
    std::uint64_t time = 0;
};

enum class Kind
{
    GuiDown, GuiMove, GuiUp, CancelGui,
    SelectBegin, SelectMove, SelectEnd, CancelSelection,
    Command, SelectSimilar, Deselect, PlacementPreview, Place, Pan, Transform
};
enum class Target { Gui, World, Placement };
struct Output
{
    Kind kind;
    Point from, to;
    float rotation = 0, zoom = 0; // radians and log(span ratio)
};

class Gestures
{
public:
    static constexpr std::uint64_t HoldMs = 420;
    static constexpr std::uint64_t DoubleTapMs = 300;
    static constexpr std::uint64_t TwoFingerJoinMs = 180;
    static constexpr std::uint64_t TwoFingerTapMs = 300;
    explicit Gestures(std::function<void(const Output&)> emit,
        std::function<bool(Point)> selectTap = {})
        : m_Emit(std::move(emit)), m_SelectTap(std::move(selectTap)) {}

    // On PointerDown both fingers must be over the same kind of game surface.
    // Placement previews never press a button until a double tap is complete.
    void Handle(const Sample& sample, Target target)
    {
        if (sample.action == Action::Cancel || !Valid(sample))
        {
            Cancel();
            return;
        }
        const Point point = sample.fingers[0].point;
        if (sample.action == Action::Down)
        {
            if (m_State != State::Idle)
                Cancel();
            if (sample.count != 1)
            {
                Cancel();
                m_State = State::Drain;
                return;
            }
            m_Density = std::clamp(sample.density, 0.25f, 10.0f);
            m_FirstId = sample.fingers[0].id;
            m_Start = m_Last = point;
            m_DownTime = sample.time;
            m_Target = target;
            m_Moved = m_MultiTap = false;
            m_Double = target != Target::Gui && target == m_TapTarget && m_Pending && sample.time >= m_TapTime &&
                sample.time - m_TapTime <= DoubleTapMs && Distance(point, m_Tap) <= 24 * m_Density;
            if (m_Pending && !m_TapSelected && !m_Double &&
                target == Target::World && m_TapTarget == Target::World)
                Emit(Kind::Command, m_Tap);
            m_Pending = false;
            m_State = target == Target::Gui ? State::Gui :
                target == Target::Placement ? State::Placement : State::Waiting;
            if (target == Target::Gui)
                Emit(Kind::GuiDown, point);
            else if (target == Target::Placement)
                Emit(Kind::PlacementPreview, point);
            return;
        }
        if (m_State == State::Idle)
            return;
        if (m_State == State::Drain)
        {
            if (sample.action == Action::Up)
                m_State = State::Idle;
            return;
        }
        if (m_State == State::MultiRelease)
        {
            // Do not deselect on the first lift: a replacement finger, a long
            // hold or a drag by the remaining finger must cancel the tap.
            if ((sample.action != Action::Move && sample.action != Action::Up) || sample.count != 1)
            {
                m_MultiTap = false;
                m_State = sample.action == Action::Up ? State::Idle : State::Drain;
                return;
            }
            CheckTapFinger(sample.fingers[0]);
            m_MultiTap = m_MultiTap && Within(sample.time, m_DownTime, TwoFingerTapMs);
            if (sample.action == Action::Up)
            {
                if (m_MultiTap)
                    Emit(Kind::Deselect, point);
                m_MultiTap = false;
                m_State = State::Idle;
            }
            return;
        }
        if (sample.action == Action::PointerDown)
        {
            m_Pending = m_Double = false;
            if ((m_State == State::Waiting || m_State == State::Panning || m_State == State::Placement) &&
                sample.count == 2 && target != Target::Gui && target == m_Target && HasFirst(sample))
            {
                const Finger first = sample.fingers[sample.fingers[0].id == m_FirstId ? 0 : 1];
                const Finger second = sample.fingers[sample.fingers[0].id == m_FirstId ? 1 : 0];
                m_MultiTap = m_State != State::Panning && !m_Moved &&
                    Within(sample.time, m_DownTime, TwoFingerJoinMs) &&
                    Distance(first.point, m_Start) <= 8 * m_Density;
                m_State = State::Multi;
                m_SecondId = second.id;
                m_SecondStart = second.point;
                ResetTransform(first.point, second.point);
            }
            else
            {
                Cancel();
                m_State = State::Drain;
            }
            return;
        }
        if (m_State == State::Multi)
        {
            if ((sample.action == Action::Move || sample.action == Action::PointerUp) && sample.count == 2)
            {
                CheckTapFinger(sample.fingers[0]);
                CheckTapFinger(sample.fingers[1]);
                m_MultiTap = m_MultiTap && Within(sample.time, m_DownTime, TwoFingerTapMs);
                if (sample.fingers[0].id == m_FirstId && sample.fingers[1].id == m_SecondId)
                    Transform(sample.fingers[0].point, sample.fingers[1].point);
                else if (sample.fingers[1].id == m_FirstId && sample.fingers[0].id == m_SecondId)
                    Transform(sample.fingers[1].point, sample.fingers[0].point);
                else
                {
                    m_MultiTap = false;
                    m_State = State::Drain;
                }
                if (sample.action == Action::PointerUp && m_State == State::Multi)
                    m_State = m_MultiTap ? State::MultiRelease : State::Drain;
            }
            else
                m_State = sample.action == Action::Up ? State::Idle : State::Drain;
            return;
        }
        if (sample.count != 1 || sample.fingers[0].id != m_FirstId ||
            sample.action == Action::PointerUp)
        {
            Cancel();
            m_State = sample.action == Action::Up ? State::Idle : State::Drain;
            return;
        }
        if (sample.action != Action::Move && sample.action != Action::Up)
            return;

        Tick(sample.time);
        const Point previous = m_Last;
        m_Last = point;
        if (m_State == State::Placement)
        {
            if (Distance(m_Start, point) > 8 * m_Density || !Within(sample.time, m_DownTime, HoldMs - 1))
            {
                m_Moved = true;
                m_Double = false;
            }
            if (Distance(previous, point) > 0)
                Emit(Kind::PlacementPreview, point);
        }
        else if (m_State == State::Waiting && Distance(m_Start, point) > 8 * m_Density)
        {
            m_State = State::Panning;
            m_Double = m_Pending = false;
            m_Emit({Kind::Pan, m_Start, point});
        }
        else if (m_State == State::Panning && Distance(previous, point) > 0)
            m_Emit({Kind::Pan, previous, point});
        else if (m_State == State::Selecting && Distance(previous, point) > 0)
            Emit(Kind::SelectMove, point);
        else if (m_State == State::Gui && sample.action == Action::Move)
            Emit(Kind::GuiMove, point);

        if (sample.action == Action::Up)
        {
            if (m_State == State::Waiting || (m_State == State::Placement && !m_Moved))
            {
                if (m_Double)
                    Emit(m_Target == Target::Placement ? Kind::Place : Kind::SelectSimilar, point);
                else
                {
                    // Selection may commit on release so its HUD opens at once.
                    // Keep the tap only for a possible double-tap upgrade; never
                    // send a delayed order after that selection has succeeded.
                    m_TapSelected = m_Target == Target::World && m_SelectTap && m_SelectTap(point);
                    m_Pending = true;
                    m_Tap = point;
                    m_TapTime = sample.time;
                    m_TapTarget = m_Target;
                }
            }
            else if (m_State == State::Selecting)
                Emit(Kind::SelectEnd, point);
            else if (m_State == State::Gui)
                Emit(Kind::GuiUp, point);
            m_State = State::Idle;
            m_Double = false;
        }
    }

    void Tick(std::uint64_t now)
    {
        if (m_State == State::Waiting && now >= m_DownTime && now - m_DownTime >= HoldMs)
        {
            m_State = State::Selecting;
            m_Double = m_Pending = false;
            Emit(Kind::SelectBegin, m_Start);
        }
        if (m_State == State::Idle && m_Pending && now >= m_TapTime && now - m_TapTime >= DoubleTapMs)
        {
            m_Pending = false;
            if (m_TapTarget == Target::World && !m_TapSelected)
                Emit(Kind::Command, m_Tap);
        }
    }

    void Cancel()
    {
        if (m_State == State::Gui)
            Emit(Kind::CancelGui, m_Last);
        else if (m_State == State::Selecting)
            Emit(Kind::CancelSelection, m_Last);
        m_State = State::Idle;
        m_Pending = m_Double = m_MultiTap = false;
        m_FirstId = m_SecondId = -1;
    }

private:
    enum class State { Idle, Waiting, Gui, Placement, Panning, Selecting, Multi, MultiRelease, Drain };
    static float Distance(Point a, Point b) { return std::hypot(a.x - b.x, a.y - b.y); }
    static bool Within(std::uint64_t now, std::uint64_t start, std::uint64_t limit)
    {
        return now >= start && now - start <= limit;
    }
    void CheckTapFinger(const Finger& finger)
    {
        if (finger.id == m_FirstId)
            m_MultiTap = m_MultiTap && Distance(finger.point, m_Start) <= 8 * m_Density;
        else if (finger.id == m_SecondId)
            m_MultiTap = m_MultiTap && Distance(finger.point, m_SecondStart) <= 8 * m_Density;
        else
            m_MultiTap = false;
    }
    static bool Valid(const Sample& sample)
    {
        if (sample.count < 1 || sample.count > 2 || !std::isfinite(sample.density) || sample.density <= 0)
            return false;
        for (int i = 0; i < sample.count; ++i)
            if (sample.fingers[i].id < 0 || !std::isfinite(sample.fingers[i].point.x) ||
                !std::isfinite(sample.fingers[i].point.y))
                return false;
        return sample.count != 2 || sample.fingers[0].id != sample.fingers[1].id;
    }
    bool HasFirst(const Sample& sample) const
    {
        return sample.fingers[0].id == m_FirstId || sample.fingers[1].id == m_FirstId;
    }
    void Emit(Kind kind, Point point) { m_Emit({kind, point, point}); }
    void ResetTransform(Point first, Point second)
    {
        m_Vector = {second.x - first.x, second.y - first.y};
        m_StartSpan = std::hypot(m_Vector.x, m_Vector.y);
        m_Rotation = m_Zoom = 0;
        m_Rotating = m_Zooming = false;
    }
    void Transform(Point first, Point second)
    {
        const Point vector{second.x - first.x, second.y - first.y};
        const float span = std::hypot(vector.x, vector.y);
        const float oldSpan = std::hypot(m_Vector.x, m_Vector.y);
        const float angle = std::atan2(m_Vector.x * vector.y - m_Vector.y * vector.x,
            m_Vector.x * vector.x + m_Vector.y * vector.y);
        // Close/crossing fingers have an unstable angle. Rebase without a jump.
        if (std::min(span, oldSpan) < 24 * m_Density || std::abs(angle) > 1.57079633f)
        {
            ResetTransform(first, second);
            return;
        }
        m_Rotation += angle;
        m_Zoom += std::log(span / oldSpan);
        m_Vector = vector;
        m_Rotating = m_Rotating || std::abs(m_Rotation) >= 0.05235988f; // 3 degrees
        m_Zooming = m_Zooming || std::abs(span - m_StartSpan) >= 8 * m_Density;
        // A short pair may drift by a few pixels while tapping. Preserve the
        // accumulated transform, but wait for touch slop or the tap timeout
        // before taking the gesture away from two-finger deselection.
        if (m_MultiTap)
            return;
        const float rotation = m_Rotating ? m_Rotation : 0;
        const float zoom = m_Zooming ? m_Zoom : 0;
        if (rotation != 0 || zoom != 0)
        {
            m_MultiTap = false;
            m_Emit({Kind::Transform, {}, {}, rotation, zoom});
        }
        if (m_Rotating) m_Rotation = 0;
        if (m_Zooming) m_Zoom = 0;
    }

    std::function<void(const Output&)> m_Emit;
    std::function<bool(Point)> m_SelectTap;
    State m_State = State::Idle;
    Target m_Target = Target::Gui, m_TapTarget = Target::Gui;
    int m_FirstId = -1, m_SecondId = -1;
    float m_Density = 1;
    Point m_Start, m_Last, m_Tap, m_Vector, m_SecondStart;
    std::uint64_t m_DownTime = 0, m_TapTime = 0;
    bool m_Pending = false, m_Double = false, m_TapSelected = false;
    bool m_Moved = false, m_MultiTap = false;
    float m_StartSpan = 0, m_Rotation = 0, m_Zoom = 0;
    bool m_Rotating = false, m_Zooming = false;
};
}
