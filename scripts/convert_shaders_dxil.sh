#!/bin/bash
set -e

VK_SDK="C:/VulkanSDK/1.4.341.1/Bin"
SRC_DIR="C:/Users/33235/Redlive/dodoe/engine/res/shaders/bin"
TMP_DIR="/tmp/dxil_build"

mkdir -p "$TMP_DIR"

declare -A STAGE_MAP
STAGE_MAP["vert"]="vert:vs_6_0"
STAGE_MAP["frag"]="frag:ps_6_0"
STAGE_MAP["geom"]="geom:gs_6_0"

count=0
for spv in "$SRC_DIR"/*.spv; do
    filename=$(basename "$spv" .spv)
    base="${filename%.*}"
    stage="${filename##*.}"

    stage_info="${STAGE_MAP[$stage]}"
    if [ -z "$stage_info" ]; then
        echo "SKIP: $spv (unknown stage: $stage)"
        continue
    fi

    spirv_stage="${stage_info%:*}"
    dxc_target="${stage_info#*:}"

    hlsl="$TMP_DIR/$base.$stage.hlsl"
    dxil="$SRC_DIR/$base.$stage.dxil"

    echo "[$((++count))] $filename.spv -> $spirv_stage -> $dxc_target"
    "$VK_SDK/spirv-cross" --hlsl --shader-model 60 --stage "$spirv_stage" --output "$hlsl" "$spv"
    "$VK_SDK/dxc" -T "$dxc_target" -E main "$hlsl" -Fo "$dxil" 2>&1
    echo "  -> $(basename "$dxil") OK"
done

echo "Done. $count shaders converted."
ls -la "$SRC_DIR"/*.dxil 2>/dev/null
