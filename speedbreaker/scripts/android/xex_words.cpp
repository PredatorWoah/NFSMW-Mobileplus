// SpeedBreaker One. GPL-3.0-or-later.
// xex_words <default.xex> <address>...: the big-endian word at each guest
// address of the loaded image, one "ADDRESS WORD" per line (check_guards.sh).
#include <file.h>
#include <image.h>

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <default.xex> <hex address>...\n", argv[0]);
        return 2;
    }
    const std::vector<uint8_t> file = LoadFile(argv[1]);
    const Image image = Image::ParseImage(file.data(), file.size());
    if (!image.data)
    {
        fprintf(stderr, "cannot load %s\n", argv[1]);
        return 1;
    }
    for (int i = 2; i < argc; i++)
    {
        const uint32_t address = uint32_t(strtoul(argv[i], nullptr, 16));
        if (address < image.base || uint64_t(address) + 4 > uint64_t(image.base) + image.size)
        {
            printf("%08X outside\n", address);
            continue;
        }
        const uint8_t* p = image.data.get() + (address - image.base);
        printf("%08X %02X%02X%02X%02X\n", address, p[0], p[1], p[2], p[3]);
    }
}
