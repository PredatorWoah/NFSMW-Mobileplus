// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See save_backup.h.
#include <stdafx.h>
#include "save_backup.h"
#include "paths.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace save_backup
{
    namespace
    {
        namespace fs = std::filesystem;
        constexpr size_t kKeep = 5;

        // Every regular file under `root`, by its path relative to it, with its bytes.
        std::vector<std::pair<std::string, std::string>> Files(const fs::path& root)
        {
            std::vector<std::pair<std::string, std::string>> files;
            std::error_code ec;
            for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
            {
                if (!it->is_regular_file(ec))
                    continue;
                std::ifstream in(it->path(), std::ios::binary);
                files.emplace_back(fs::relative(it->path(), root, ec).generic_string(),
                    std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()));
            }
            std::sort(files.begin(), files.end());
            return files;
        }
    }

    void BackUp()
    {
        const fs::path user = GetUserPath(), save = user / "save", backups = user / "save-backups";
        std::error_code ec;
        const auto now = Files(save);
        if (now.empty())
            return;
        std::vector<fs::path> kept;
        for (auto it = fs::directory_iterator(backups, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
            if (it->is_directory(ec))
                kept.push_back(it->path());
        std::sort(kept.begin(), kept.end());  // the names are dates: oldest first
        if (!kept.empty() && Files(kept.back()) == now)
        {
            fprintf(stderr, "[save-backup] saves unchanged since %s\n", kept.back().filename().string().c_str());
            return;
        }
        char stamp[32];
        const time_t t = time(nullptr);
        strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", localtime(&t));
        const fs::path to = backups / stamp, partial = backups / (std::string(stamp) + ".partial");
        fs::remove_all(partial, ec);
        fs::create_directories(partial, ec);
        fs::copy(save, partial, fs::copy_options::recursive, ec);
        if (ec || (fs::rename(partial, to, ec), ec))
        {
            fprintf(stderr, "[save-backup] couldn't back up %s: %s\n", save.string().c_str(), ec.message().c_str());
            fs::remove_all(partial, ec);
            return;
        }
        kept.push_back(to);
        while (kept.size() > kKeep)
        {
            fs::remove_all(kept.front(), ec);
            kept.erase(kept.begin());
        }
        fprintf(stderr, "[save-backup] %zu file(s) backed up to save-backups/%s (%zu kept)\n", now.size(), stamp, kept.size());
    }
}
