// Synthetic XDVDFS image: real parser, bounded readers and runtime mount.
#include <install/disc_mount.h>
#include <cassert>
#include <cstring>
#include <thread>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    std::vector<uint8_t> image(39 * 2048);
    auto le16 = [&](size_t at, uint16_t n) { image[at] = uint8_t(n); image[at + 1] = uint8_t(n >> 8); };
    auto le32 = [&](size_t at, uint32_t n) { for (int i = 0; i < 4; ++i) image[at + i] = uint8_t(n >> (8 * i)); };
    auto node = [&](size_t at, uint16_t right, uint32_t sector, uint32_t size, uint8_t attributes, const char* name) {
        le16(at + 2, right); le32(at + 4, sector); le32(at + 8, size);
        image[at + 12] = attributes; image[at + 13] = uint8_t(strlen(name));
        memcpy(image.data() + at + 14, name, strlen(name));
    };
    memcpy(image.data() + 32 * 2048, "MICROSOFT*XBOX*MEDIA", 20);
    le32(32 * 2048 + 20, 33); le32(32 * 2048 + 24, 48);
    node(33 * 2048, 5, 34, 56, 0x10, "NFS");
    node(33 * 2048 + 20, 0, 36, 4, 0, "default.xex");
    node(34 * 2048, 7, 37, 12, 0, "TEXTURES.BIN");
    node(34 * 2048 + 28, 0, 38, 9, 0, "SOUND.BIN");
    memcpy(image.data() + 36 * 2048, "XEX2", 4);
    memcpy(image.data() + 37 * 2048, "texture-data", 12);
    memcpy(image.data() + 38 * 2048, "soundfile", 9);
    char name[] = "/tmp/sb-disc-mount-XXXXXX";
    int fd = mkstemp(name); assert(fd >= 0);
    assert(write(fd, image.data(), image.size()) == ssize_t(image.size()));
    std::unique_ptr<install::DiscSource> source;
    auto result = install::OpenDiscSource(name, source);
    assert(result.Ok() && source && source->Files().size() == 3);
    result = install::OpenDiscImageDescriptor(fd, "test-disc.iso", source);
    assert(result.Ok() && source && source->Files().size() == 3);
    close(fd);
    // Unlink after opening: all subsequent data must come from held descriptors.
    assert(unlink(name) == 0);
    std::shared_ptr<install::DiscSource> retained = std::move(source);
    std::string error;
    assert(discmount::Mount(retained, error));
    const auto root = discmount::Root();
    assert(discmount::Stat(root)->directory);
    assert(discmount::Children(root).size() == 2);
    assert(discmount::Children(root / "nfs").size() == 2);
    auto file = discmount::Resolve(root / "nfs/textures.bin");
    assert(file && file->filename() == "TEXTURES.BIN");
    assert(discmount::Stat(*file)->size == 12);
    assert(!discmount::Stat(*file)->directory);
    assert(!discmount::Resolve(root / "missing.bin"));
    assert(!discmount::Contains(root / "../outside"));
    assert(!discmount::Contains(root.string() + "-other/file"));
    assert(!discmount::Open(root / "NFS", error));
    auto reader = discmount::Open(*file, error); assert(reader);
    char data[12]{};
    assert(reader->Read(2, data, 7, error) && !memcmp(data, "xture-d", 7));
    assert(reader->Read(12, data, 0, error));
    assert(!reader->Read(12, data, 1, error));
    assert(!reader->Read(UINT64_MAX, data, 1, error));
    auto readMany = [&](const char* relative, const char* expected, size_t size) {
        std::string localError;
        auto concurrent = discmount::Open(root / relative, localError); assert(concurrent);
        for (int i = 0; i < 1000; ++i) {
            char bytes[12]{};
            assert(concurrent->Read(0, bytes, size, localError));
            assert(!memcmp(bytes, expected, size));
        }
    };
    std::thread a(readMany, "nfs/textures.bin", "texture-data", 12);
    std::thread b(readMany, "NFS/SOUND.BIN", "soundfile", 9);
    a.join(); b.join();
    assert(!discmount::Mount(retained, error)); // no replacing a live guest mount
    puts("PASS: ISO mount, case folding, directories, descriptor lifetime, read bounds and concurrent readers");
}
