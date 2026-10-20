// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See cheats.h.
#include <stdafx.h>
#include "cheats.h"

#include <kernel/memory.h>
#include <user/paths.h>

#include <cstdio>
#include <fstream>
#include <set>
#include <string>

namespace
{
    struct DataPatch
    {
        uint32_t address;  // guest, big-endian 32-bit word
        uint32_t value;
        // The values found there that are safe to replace: the original
        // and the patched one (already applied, e.g. a second start).
        uint32_t expected[2];
    };

    struct Cheat
    {
        const char* id;    // the key in cheats.txt
        const char* name;  // for the log
        DataPatch patches[4];
        int count;
    };

    // Black Edition content: the bonus cars and races of the Collector's
    // Edition. From Xenia's game patches for title 454107D9
    // (xenia-canary/game-patches, "Black Edition content", by tuhana), which
    // lists the NTSC-U default.xex (FB9D1825EAC7AA75) among its modules. The
    // word is in the image's data, so it is a plain data write here too.
    // The original value is assumed 0 (the edition off); anything else is
    // left alone and logged.
    constexpr Cheat kCheats[] = {
        { "black_edition", "Black Edition content", { { 0x82A2CE04, 0x00000100, { 0x00000000, 0x00000100 } } }, 1 },
    };

    std::set<std::string> ReadEnabled(const std::filesystem::path& path)
    {
        std::set<std::string> on;
        std::ifstream in(path);
        for (std::string line; std::getline(in, line);)
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
                line.pop_back();
            size_t eq = line.find('=');
            if (line.empty() || line[0] == '#' || eq == std::string::npos)
                continue;
            if (line.substr(eq + 1) == "1")
                on.insert(line.substr(0, eq));
        }
        return on;
    }
}

namespace cheats
{
    void ApplyStartup()
    {
        const std::filesystem::path path = GetUserPath() / "cheats.txt";
        const std::set<std::string> on = ReadEnabled(path);
        if (on.empty())
            return;
        std::set<std::string> known;
        for (const Cheat& cheat : kCheats)
        {
            known.insert(cheat.id);
            if (!on.count(cheat.id))
                continue;
            bool applied = true;
            for (int i = 0; i < cheat.count; i++)
            {
                const DataPatch& p = cheat.patches[i];
                auto* word = static_cast<uint32_t*>(g_memory.Translate(p.address));
                const uint32_t now = ByteSwap(*word);
                bool safe = false;
                for (uint32_t v : p.expected)
                    safe |= now == v;
                if (!safe)
                {
                    fprintf(stderr, "[cheats] %s: %08X holds %08X, not an expected value; NOT patched "
                        "(another executable?)\n", cheat.name, p.address, now);
                    applied = false;
                    continue;
                }
                *word = ByteSwap(p.value);
            }
            fprintf(stderr, "[cheats] %s: %s\n", cheat.name, applied ? "on" : "skipped");
        }
        for (const std::string& id : on)
            if (!known.count(id))
                fprintf(stderr, "[cheats] unknown cheat '%s' in %s, ignored\n", id.c_str(), path.string().c_str());
    }
}
