#version 450 core

// UI vertex shader — instance-drawn quads with position/size/uv/color

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in uint a_TexIndex;
layout(location = 4) in vec4 i_Transform;  // position.xy, size.xy
layout(location = 5) in vec4 i_UVRect;     // uv_min.xy, uv_max.xy
layout(location = 6) in vec4 i_ColorDepth; // color, atlas_index, depth, flags
layout(location = 7) in vec4 i_ClipRect;   // clip_rect (unused in VS)

layout(location = 0) out vec2 v_UV;
layout(location = 1) out vec4 v_Color;
layout(location = 2) flat out uint v_TexIndex;

layout(set = 0, binding = 256) uniform UIVP
{
    mat4 u_ViewProjection;
};

vec4 unpackRGBA8(uint packed)
{
    return vec4(
        float((packed >> 0u)  & 0xFFu) / 255.0,
        float((packed >> 8u)  & 0xFFu) / 255.0,
        float((packed >> 16u) & 0xFFu) / 255.0,
        float((packed >> 24u) & 0xFFu) / 255.0
    );
}

void main()
{
    vec2 position    = i_Transform.xy;
    vec2 size        = i_Transform.zw;
    vec2 uv_min      = i_UVRect.xy;
    vec2 uv_max      = i_UVRect.zw;
    uint packedColor = floatBitsToUint(i_ColorDepth.x);
    uint texIndex    = floatBitsToUint(i_ColorDepth.y);
    float depth      = i_ColorDepth.z;

    vec2 worldPos = a_Position.xy * size + position;

    v_UV = vec2(mix(uv_min.x, uv_max.x, a_UV.x),
                mix(uv_min.y, uv_max.y, a_UV.y));
    v_Color = unpackRGBA8(packedColor);
    v_TexIndex = texIndex;
    gl_Position = u_ViewProjection * vec4(worldPos, depth, 1.0);
}
