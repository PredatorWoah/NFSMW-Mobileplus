// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include <SDL3/SDL_system.h>
#include <filesystem>
namespace platform::android {
inline std::filesystem::path Files() { const char* p = SDL_GetAndroidInternalStoragePath(); return p ? p : "."; }
}
