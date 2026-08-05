#version 450 core

// UI bindless pixel shader — texture array variant

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec2 v_UV;
layout(location = 1) in vec4 v_Color;
layout(location = 2) flat in uint v_TexIndex;

const uint kMaxTextures = 1024u;
layout(set = 1, binding = 0) uniform sampler u_TextureSampler;
layout(set = 3, binding = 0) uniform texture2D u_Textures[kMaxTextures];

void main()
{
    uint textureIndex = min(v_TexIndex, kMaxTextures - 1u);
    vec4 texColor = texture(sampler2D(u_Textures[textureIndex], u_TextureSampler), v_UV);
    o_Color = texColor * v_Color;
}
