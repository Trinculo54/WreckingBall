#!/usr/bin/env bash
# Cross-compiles WreckingBall with llvm-mingw and stages dist/:
#   dist/d3dcompiler_47.dll           FH2 Mod Platform (next to azimuth.exe)
#   dist/fh2-mods/WreckingBall.dll    the mod
#   dist/README.txt, dist/LICENSE.txt, dist/LICENSE-MinHook.txt (the platform includes MinHook)
#
#   $ scripts/build.sh
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
if [ ! -f "$root/platform/CMakeLists.txt" ]; then
    git -C "$root" submodule update --init platform
fi
for d in /opt/llvm-mingw/bin /usr/local/llvm-mingw/bin; do [ -d "$d" ] && PATH=$d:$PATH; done
cmake -S "$root" -B "$root/build" -DCMAKE_TOOLCHAIN_FILE="$root/cmake/mingw-w64-x86_64.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build "$root/build" -j "${JOBS:-$(nproc)}"
rm -rf "$root/dist"
mkdir -p "$root/dist/fh2-mods"
cp "$root/build/platform/d3dcompiler_47.dll" "$root/dist/"
cp "$root/build/WreckingBall.dll" "$root/dist/fh2-mods/"
cp "$root/scripts/dist-readme.txt" "$root/dist/README.txt"
cp "$root/LICENSE" "$root/dist/LICENSE.txt"
cp "$root/platform/third_party/minhook/LICENSE.txt" "$root/dist/LICENSE-MinHook.txt"
(cd "$root/dist" && find . -type f -printf '  %P\n')
