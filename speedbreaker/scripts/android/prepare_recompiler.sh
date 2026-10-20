#!/usr/bin/env bash
# Fetch the exact upstream toolchain and preserve every SpeedBreaker patch.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
XR="$ROOT/tools/XenonRecomp"
BASE=ddd128bcca99fe8bfbb99bea583c972351fa6ace
if [ ! -d "$XR" ]; then git clone --recursive https://github.com/hedge-dev/XenonRecomp.git "$XR"; fi
if ! git -C "$XR" rev-parse --verify nfsmw >/dev/null 2>&1; then
    git -C "$XR" switch -c nfsmw "$BASE"
    git -C "$XR" submodule update --init --recursive
    git -C "$XR" -c user.name='SpeedBreaker setup' -c user.email=setup@speedbreaker.invalid am "$ROOT"/patches/xenonrecomp/*.patch
fi
# Existing clones must be on the patched branch; never silently use main.
if [ "$(git -C "$XR" branch --show-current)" != nfsmw ]; then
    echo 'XenonRecomp must be on the patched nfsmw branch' >&2; exit 1
fi
if [ "${1:-}" = --headers-only ]; then exit 0; fi
cmake -Wno-deprecated -S "$XR" -B "$XR/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="${CC:-clang}" -DCMAKE_CXX_COMPILER="${CXX:-clang++}" '-DCMAKE_CXX_FLAGS=-include cstdlib'
cmake --build "$XR/build" --target XenonRecomp XenonAnalyse -j "${JOBS:-6}"
