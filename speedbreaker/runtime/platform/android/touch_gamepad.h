// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include <SDL3/SDL.h>
namespace platform::android::touch {
void Initialize();
void Update();
void Clear();
void SetBlocked(bool blocked);
void Shutdown();
bool IsPad(SDL_JoystickID id);
}
