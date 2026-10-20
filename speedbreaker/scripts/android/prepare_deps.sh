#!/usr/bin/env bash
# SpeedBreaker One. GPL-3.0-or-later. Android native dependencies only.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to NDK r28c}"
CMAKE="${CMAKE:-cmake}"
PREFIX="$ROOT/tools/android-arm64"
JOBS="${JOBS:-6}"
mkdir -p "$ROOT/tools" "$PREFIX"
if [ ! -d "$ROOT/tools/SDL" ]; then
    git clone --depth 1 --branch release-3.2.24 https://github.com/libsdl-org/SDL.git "$ROOT/tools/SDL"
fi
# SDL is built by android/CMakeLists.txt, and its matching Java is used by Gradle.
if [ "${1:-}" = "--probe-only" ]; then exit 0; fi
if [ ! -d "$ROOT/tools/glslang-android" ]; then
    git clone --depth 1 --branch 15.1.0 https://github.com/KhronosGroup/glslang.git "$ROOT/tools/glslang-android"
fi
# glslang 15.1.0's known_good.json pins these revisions. The Android driver
# needs optimized SPIR-V; merely requesting optimization in the C API does
# nothing when glslang was built with ENABLE_OPT=OFF.
checkout_pinned() {
    local directory="$1" repository="$2" revision="$3"
    if [ ! -d "$directory/.git" ]; then
        mkdir -p "$directory"
        git init -q "$directory"
    fi
    if ! git -C "$directory" cat-file -e "$revision^{commit}" 2>/dev/null; then
        git -C "$directory" fetch --depth 1 "$repository" "$revision"
    fi
    git -C "$directory" checkout --detach "$revision"
}
# Rootless driver loading and matching global Vulkan dispatch (source only).
checkout_pinned "$ROOT/tools/libadrenotools" https://github.com/bylaws/libadrenotools.git 8fae8ce254dfc1344527e05301e43f37dea2df80
git -C "$ROOT/tools/libadrenotools" submodule update --init --recursive
checkout_pinned "$ROOT/tools/volk" https://github.com/zeux/volk.git 43c29e655cb8117fd9cfb65ad9cefe2d40965102
checkout_pinned "$ROOT/tools/glslang-android/External/spirv-tools" \
    https://github.com/KhronosGroup/SPIRV-Tools.git 4d2f0b40bfe290dea6c6904dafdf7fd8328ba346
checkout_pinned "$ROOT/tools/glslang-android/External/spirv-tools/external/spirv-headers" \
    https://github.com/KhronosGroup/SPIRV-Headers.git 3f17b2af6784bfa2c5aa5dbb8e0e74a607dd8b3b
if [ "${1:-}" = "--sources-only" ]; then exit 0; fi
"$CMAKE" -S "$ROOT/tools/glslang-android" -B "$ROOT/build/glslang-android" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-33 -DANDROID_STL=c++_shared \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DENABLE_GLSLANG_BINARIES=OFF \
    -DENABLE_OPT=ON -DBUILD_TESTING=OFF -DGLSLANG_TESTS=OFF \
    -DSPIRV_SKIP_TESTS=ON -DSPIRV_SKIP_EXECUTABLES=ON
"$CMAKE" --build "$ROOT/build/glslang-android" -j "$JOBS"
"$CMAKE" --install "$ROOT/build/glslang-android"
TARBALL="$ROOT/tools/ffmpeg-src/ffmpeg-7.1.1.tar.xz"
mkdir -p "$(dirname "$TARBALL")"
if [ ! -f "$TARBALL" ]; then curl -fL --retry 3 https://ffmpeg.org/releases/ffmpeg-7.1.1.tar.xz -o "$TARBALL"; fi
sha256_check() {
    if command -v sha256sum >/dev/null; then sha256sum -c -; else shasum -a 256 -c -; fi
}
printf '%s  %s\n' 733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1 "$TARBALL" | sha256_check
# FFmpeg checks that no absolute machine paths are embedded: compiler names
# must be relative, resolved from PATH, not absolute --cc/--ar arguments.
# Its AArch64 assembly addresses internal tables directly. Hide those C symbols
# so the static decoder can link into Android's shared libmain.so without
# requiring symbol interposition relocations.
case "$(uname -s)" in
    Darwin) NDK_HOST=darwin-x86_64 ;;
    Linux) NDK_HOST=linux-x86_64 ;;
    *) echo 'Unsupported Android build host' >&2; exit 1 ;;
esac
export PATH="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$NDK_HOST/bin:$PATH"
"$ROOT/scripts/build_ffmpeg_xma.sh" "$TARBALL" "$PREFIX/ffmpeg-xma" \
    --enable-cross-compile --target-os=android --arch=aarch64 \
    --extra-cflags=-fvisibility=hidden \
    --cc=aarch64-linux-android33-clang --cxx=aarch64-linux-android33-clang++ \
    --ar=llvm-ar --ranlib=llvm-ranlib --strip=llvm-strip
