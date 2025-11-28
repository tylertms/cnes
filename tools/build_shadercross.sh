#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.."; pwd)"
SCROOT="$ROOT/tools/shadercross"
BUILD="$SCROOT/build"

rm -rf "$SCROOT"
mkdir -p "$BUILD"

git clone --recurse-submodules https://github.com/libsdl-org/SDL_shadercross.git "$BUILD/src"

cd "$BUILD/src"

cmake -S . -B build -G Ninja \
  -DSDLSHADERCROSS_CLI=ON \
  -DSDLSHADERCROSS_VENDORED=ON \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build --target shadercross --parallel

cp build/shadercross "$SCROOT/shadercross"
chmod +x "$SCROOT/shadercross"

SPV_LIB="$(find build -name 'libspirv-cross-c-shared.*' | head -n 1 || true)"
DXC_LIB="$(find build -name 'libdxcompiler.*' | head -n 1 || true)"

if [ -n "$SPV_LIB" ]; then
  cp "$SPV_LIB" "$SCROOT/"
  SPV_NAME="$(basename "$SPV_LIB")"
fi

if [ -n "$DXC_LIB" ]; then
  cp "$DXC_LIB" "$SCROOT/"
  DXC_NAME="$(basename "$DXC_LIB")"
fi

if [[ "$OSTYPE" == "darwin"* ]]; then
  if [ -n "${SPV_LIB:-}" ]; then
    install_name_tool -change @rpath/libspirv-cross-c-shared.0.dylib "@executable_path/$SPV_NAME" "$SCROOT/shadercross"
  fi
  if [ -n "${DXC_LIB:-}" ]; then
    install_name_tool -change @rpath/libdxcompiler.dylib "@executable_path/$DXC_NAME" "$SCROOT/shadercross"
  fi
fi

rm -rf "$BUILD"
