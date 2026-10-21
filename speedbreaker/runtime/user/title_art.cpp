// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See title_art.h.
#include <stdafx.h>
#include "title_art.h"
#include "paths.h"

#include <xdbf_wrapper.h>
#include <xex.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

namespace title_art
{
    namespace
    {
        // Writes `data` to `path` unless it already holds exactly that.
        bool Write(const std::filesystem::path& path, const void* data, size_t size)
        {
            std::ifstream in(path, std::ios::binary);
            if (in)
            {
                std::string old((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                if (old.size() == size && memcmp(old.data(), data, size) == 0)
                    return false;
            }
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out.write(static_cast<const char*>(data), std::streamsize(size));
            return bool(out.flush());
        }
    }

    void Save(const std::vector<uint8_t>& file, const uint8_t* image, uint32_t base, uint32_t size)
    {
        // The resource table: its total size (big endian), then 16-byte
        // entries: an 8-character name, the address in the image, the size.
        auto* table = static_cast<const uint8_t*>(getOptHeaderPtr(file.data(), XEX_HEADER_RESOURCE_INFO));
        if (!table)
            return;
        const uint32_t total = ByteSwap(*reinterpret_cast<const uint32_t*>(table));
        for (uint32_t at = 4; at + 16 <= total; at += 16)
        {
            const uint8_t* entry = table + at;
            const uint32_t address = ByteSwap(*reinterpret_cast<const uint32_t*>(entry + 8));
            const uint32_t length = ByteSwap(*reinterpret_cast<const uint32_t*>(entry + 12));
            if (address < base || length < 4 || uint64_t(address) + length > uint64_t(base) + size)
                continue;
            const uint8_t* data = image + (address - base);
            if (memcmp(data, "XDBF", 4) != 0)
                continue;
            XDBFWrapper xdbf(data, length);
            if (!xdbf.pBuffer)
                continue;
            const XDBFBlock icon = xdbf.GetResource(XDBF_SPA_NAMESPACE_IMAGE, 0x8000);
            const std::string name = xdbf.GetString(XDBF_LANGUAGE_ENGLISH, 0x8000);
            if (!icon || icon.BufferSize < 8 || memcmp(icon.pBuffer, "\x89PNG", 4) != 0)
                continue;
            const std::filesystem::path user = GetUserPath();
            const bool newIcon = Write(user / "title-icon.png", icon.pBuffer, icon.BufferSize);
            const bool newName = !name.empty() && Write(user / "title.txt", name.data(), name.size());
            fprintf(stderr, "[title-art] \"%s\": %zu-byte icon from the disc's XDBF%s\n", name.c_str(), icon.BufferSize,
                newIcon || newName ? ", saved for the launcher" : "");
            return;
        }
    }
}
