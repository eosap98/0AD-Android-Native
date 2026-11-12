// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

struct SDL_Event_;
namespace AndroidTouch
{
struct CameraMotion { float x = 0, y = 0, rotation = 0, zoom = 0; };
void Frame();
void OtherInput(const SDL_Event_* event);
CameraMotion ConsumeCameraMotion();
bool UsingTouch();
}
