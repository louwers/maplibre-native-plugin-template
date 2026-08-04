#!/usr/bin/env sh
set -eu

ndk_root=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

case "$(uname -s)" in
  Darwin) host_tag=darwin-x86_64 ;;
  Linux) host_tag=linux-x86_64 ;;
  *) echo "Unsupported shader host" >&2; exit 1 ;;
esac

glslc="$ndk_root/shader-tools/$host_tag/glslc"
if [ ! -x "$glslc" ]; then
  echo "glslc was not found at $glslc" >&2
  exit 1
fi

compile() {
  source_name=$1
  symbol_name=$2
  "$glslc" -O "$script_dir/$source_name" -o "$script_dir/$source_name.spv"
  xxd -i -n "$symbol_name" "$script_dir/$source_name.spv" "$script_dir/$source_name.spv.inc"
  sed -i.bak \
    -e 's/^unsigned char /alignas(4) static const unsigned char /' \
    -e 's/^unsigned int /static const unsigned int /' \
    "$script_dir/$source_name.spv.inc"
  rm "$script_dir/$source_name.spv.inc.bak"
}

compile_variant() {
  source_name=$1
  output_name=$2
  symbol_name=$3
  base_attribute=$4
  height_attribute=$5
  "$glslc" -O \
    -DUSE_BASE_ATTRIBUTE="$base_attribute" \
    -DUSE_HEIGHT_ATTRIBUTE="$height_attribute" \
    "$script_dir/$source_name" \
    -o "$script_dir/$output_name.spv"
  xxd -i -n "$symbol_name" "$script_dir/$output_name.spv" "$script_dir/$output_name.spv.inc"
  sed -i.bak \
    -e 's/^unsigned char /alignas(4) static const unsigned char /' \
    -e 's/^unsigned int /static const unsigned int /' \
    "$script_dir/$output_name.spv.inc"
  rm "$script_dir/$output_name.spv.inc.bak"
}

for base_attribute in 0 1; do
  for height_attribute in 0 1; do
    variant="b${base_attribute}_h${height_attribute}"
    compile_variant shadow_roof.vert "shadow_roof_${variant}.vert" "shadow_roof_${variant}_vert_spv" \
      "$base_attribute" "$height_attribute"
    compile_variant shadow_wall.vert "shadow_wall_${variant}.vert" "shadow_wall_${variant}_vert_spv" \
      "$base_attribute" "$height_attribute"
  done
done
compile shadow_mask.frag shadow_mask_frag_spv
compile shadow_composite.vert shadow_composite_vert_spv
compile shadow_composite.frag shadow_composite_frag_spv
