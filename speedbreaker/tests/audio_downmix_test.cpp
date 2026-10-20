// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
// Functional check against pinned SDL: every guest 5.1 channel survives
// stereo playback. Uses dummy audio, without a game or physical device.
#include <SDL3/SDL.h>
#include <cassert>
#include <cmath>
#include <vector>
#include <iostream>

int main()
{
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    assert(SDL_InitSubSystem(SDL_INIT_AUDIO));
    SDL_AudioSpec stereo{SDL_AUDIO_F32, 2, 48000};
    SDL_AudioSpec guest{SDL_AUDIO_F32, 6, 48000};
    auto* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &stereo, nullptr, nullptr);
    assert(stream);
    assert(SDL_SetAudioStreamFormat(stream, &guest, nullptr));
    SDL_AudioSpec src{}, dst{}, device{};
    assert(SDL_GetAudioStreamFormat(stream, &src, &dst));
    assert(SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream), &device, nullptr));
    assert(src.channels == 6 && dst.channels == 2 && device.channels == 2);
    constexpr int frames = 1024;
    for (int channel = 0; channel < 6; ++channel)
    {
        assert(SDL_ClearAudioStream(stream));
        std::vector<float> in(frames * 6, 0), out(frames * 2, 0);
        for (int i = 0; i < frames; ++i) in[i * 6 + channel] = 0.5f;
        assert(SDL_PutAudioStreamData(stream, in.data(), int(in.size() * sizeof(float))));
        assert(SDL_FlushAudioStream(stream));
        int bytes = SDL_GetAudioStreamData(stream, out.data(), int(out.size() * sizeof(float)));
        assert(bytes == int(out.size() * sizeof(float)));
        double left = 0, right = 0;
        for (int i = 0; i < frames; ++i)
        {
            assert(std::isfinite(out[i * 2]) && std::isfinite(out[i * 2 + 1]));
            left += std::fabs(out[i * 2]); right += std::fabs(out[i * 2 + 1]);
        }
        assert(left + right > 0.01); // includes FC, LFE, SL and SR
        if (channel == 0) assert(left > right);
        if (channel == 1) assert(right > left);
        if (channel == 2 || channel == 3) assert(std::fabs(left - right) < 0.001);
        if (channel == 4) assert(left > right);
        if (channel == 5) assert(right > left);
    }
    SDL_DestroyAudioStream(stream);
    SDL_Quit();
    std::cout << "All six guest channels survive stereo playback\n";
}
