// SpeedBreaker. GPL-3.0-or-later.
#pragma once
#include <install/disc_source.h>
#include <functional>
#include <optional>

namespace platform::android
{
    struct SelectedDisc
    {
        std::shared_ptr<install::DiscSource> source;
        std::vector<uint8_t> xex;
        std::string name;
    };
    std::optional<SelectedDisc> ChooseDisc(const std::function<bool()>& runFrame);
    void RememberDisc();
}
