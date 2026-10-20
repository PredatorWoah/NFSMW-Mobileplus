// SpeedBreaker. GPL-3.0-or-later. Read-only runtime view of an XDVDFS image.
#pragma once
#include "disc_source.h"
#include <optional>

namespace discmount
{
    struct Attributes { uint64_t size; bool directory; };
    const std::filesystem::path& Root();
    // Set once before guest threads start; never replace a live guest mount.
    bool Mount(std::shared_ptr<install::DiscSource> source, std::string& error);
    bool Active();
    bool Contains(const std::filesystem::path& path);
    std::optional<std::filesystem::path> Resolve(const std::filesystem::path& path);
    std::optional<Attributes> Stat(const std::filesystem::path& path);
    std::vector<std::filesystem::path> Children(const std::filesystem::path& path);
    std::unique_ptr<install::FileReader> Open(const std::filesystem::path& path, std::string& error);
}
