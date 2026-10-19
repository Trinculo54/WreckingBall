#!/usr/bin/env bash
# Cross-compiles WreckingBall with llvm-mingw and stages dist/ as one FH2ML mod folder,
# ready to zip (the zip extracts straight into the Azimuth folder, and the FH2ML Manager's
# Install mod takes it too):
#   dist/mods/wreckingball/mod.json           generated from CMakeLists.txt's version
#   dist/mods/wreckingball/WreckingBall.dll   the mod
#   dist/mods/wreckingball/README.txt, LICENSE.txt
#
#   $ scripts/build.sh
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
for d in /opt/llvm-mingw/bin /usr/local/llvm-mingw/bin; do [ -d "$d" ] && PATH=$d:$PATH; done
cmake -S "$root" -B "$root/build" -DCMAKE_TOOLCHAIN_FILE="$root/cmake/mingw-w64-x86_64.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build "$root/build" -j "${JOBS:-$(nproc)}"
mod="$root/dist/mods/wreckingball"
rm -rf "$root/dist"
mkdir -p "$mod"
cp "$root/build/mod.json" "$mod/"
cp "$root/build/WreckingBall.dll" "$mod/"
cp "$root/scripts/dist-readme.txt" "$mod/README.txt"
cp "$root/LICENSE" "$mod/LICENSE.txt"
(cd "$root/dist" && find . -type f -printf '  %P\n' | sort)
