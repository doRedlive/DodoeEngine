#version 450 core

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec2 v_UV;
layout(location = 1) in vec4 v_Color;
layout(location = 2) flat in uint v_TexIndex;

const uint kMaxTextures = 1024u;
layout(set = 0, binding = 0) uniform sampler2D u_Textures[kMaxTextures];

void main()
{
    uint textureIndex = min(v_TexIndex, kMaxTextures - 1u);
    vec4 texColor = texture(u_Textures[textureIndex], v_UV);
    vec4 color = texColor * v_Color;
    if (color.a <= 0.0)
        discard;
    o_Color = color;
}
