#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.."; pwd)"
SCROSS="$ROOT/tools/shadercross/shadercross"
SHADERS="$ROOT/shaders"
OUT="$SHADERS/generated"

mkdir -p "$OUT"

glslangValidator -V "$SHADERS/nes.vert" -o "$OUT/nes_vert.spv"
glslangValidator -V "$SHADERS/nes.frag" -o "$OUT/nes_frag.spv"

"$SCROSS" "$OUT/nes_vert.spv" -s SPIRV -d MSL -t vertex -o "$OUT/nes_vert.msl"
"$SCROSS" "$OUT/nes_frag.spv" -s SPIRV -d MSL -t fragment -o "$OUT/nes_frag.msl"

gen_shader_header() {
  local in="$1"
  local out="$2"
  local sym="$3"

  {
    echo "#pragma once"
    echo "#include <stdint.h>"
    echo
    xxd -i -n "$sym" "$in" \
      | sed -e 's/^unsigned char /static const uint8_t /' \
            -e 's/^unsigned int /static const uint32_t /'
  } > "$out"
}

gen_shader_header "$OUT/nes_vert.spv" "$OUT/nes_vert_spv.h" nes_vert_spv
gen_shader_header "$OUT/nes_frag.spv" "$OUT/nes_frag_spv.h" nes_frag_spv
gen_shader_header "$OUT/nes_vert.msl" "$OUT/nes_vert_msl.h" nes_vert_msl
gen_shader_header "$OUT/nes_frag.msl" "$OUT/nes_frag_msl.h" nes_frag_msl

rm -rf $OUT/nes_vert.spv $OUT/nes_frag.spv $OUT/nes_vert.msl $OUT/nes_frag.msl
