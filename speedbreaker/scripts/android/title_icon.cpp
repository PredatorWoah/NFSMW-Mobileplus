// SpeedBreaker One. GPL-3.0-or-later.
// title_icon <default.xex> <out.png>: the game's dashboard icon (XDBF image
// 0x8000, 64x64 PNG) from the player's own default.xex, for the game APK's
// launcher icon (prepare_disc_icon.sh). The same lookup as the runtime's
// user/title_art.cpp. Its output never goes into the repository.
#include <file.h>
#include <image.h>
#include <xdbf_wrapper.h>
#include <xex.h>

#include <cstdio>
#include <cstring>
#include <fstream>

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <default.xex> <out.png>\n", argv[0]);
        return 2;
    }
    const std::vector<uint8_t> file = LoadFile(argv[1]);
    if (file.empty())
    {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }
    const Image image = Image::ParseImage(file.data(), file.size());
    auto* table = static_cast<const uint8_t*>(getOptHeaderPtr(file.data(), XEX_HEADER_RESOURCE_INFO));
    if (!image.data || !table)
    {
        fprintf(stderr, "no resources in %s\n", argv[1]);
        return 1;
    }
    const uint32_t total = ByteSwap(*reinterpret_cast<const uint32_t*>(table));
    for (uint32_t at = 4; at + 16 <= total; at += 16)
    {
        const uint8_t* entry = table + at;
        const uint32_t address = ByteSwap(*reinterpret_cast<const uint32_t*>(entry + 8));
        const uint32_t length = ByteSwap(*reinterpret_cast<const uint32_t*>(entry + 12));
        if (address < image.base || uint64_t(address) + length > uint64_t(image.base) + image.size)
            continue;
        const uint8_t* data = image.data.get() + (address - image.base);
        if (length < 4 || memcmp(data, "XDBF", 4) != 0)
            continue;
        const XDBFBlock icon = XDBFWrapper(data, length).GetResource(XDBF_SPA_NAMESPACE_IMAGE, 0x8000);
        if (!icon || icon.BufferSize < 8 || memcmp(icon.pBuffer, "\x89PNG", 4) != 0)
            continue;
        std::ofstream(argv[2], std::ios::binary).write(reinterpret_cast<const char*>(icon.pBuffer), std::streamsize(icon.BufferSize));
        printf("%s: %zu-byte title icon\n", argv[2], icon.BufferSize);
        return 0;
    }
    fprintf(stderr, "no title icon in %s\n", argv[1]);
    return 1;
}
