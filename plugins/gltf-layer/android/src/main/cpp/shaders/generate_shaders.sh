#!/usr/bin/env bash
set -euo pipefail

ndk_root="$1"
script_dir="$(cd "$(dirname "$0")" && pwd)"
glslc="$ndk_root/shader-tools/darwin-x86_64/glslc"
if [[ ! -x "$glslc" ]]; then
  glslc="$ndk_root/shader-tools/linux-x86_64/glslc"
fi

for shader in gltf.vert gltf.frag; do
  "$glslc" "$script_dir/$shader" -o "$script_dir/$shader.spv"
  symbol="$(printf '%s' "$shader" | tr '.-' '__')_spv"
  xxd -i -n "$symbol" "$script_dir/$shader.spv" > "$script_dir/$shader.spv.inc"
done
