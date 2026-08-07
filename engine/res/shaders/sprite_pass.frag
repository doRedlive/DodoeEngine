#version 450 core

#include "shader_parameter_sets.glsl"

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec2 v_UV;
layout(location = 1) in vec4 v_Color;
layout(location = 2) flat in uint v_TexIndex;

const uint kMaxTextures = 1024u;
layout(set = DOE_SET_MATERIAL, binding = DOE_MATERIAL_BINDING_SAMPLER) uniform sampler u_TextureSampler;
layout(set = DOE_SET_BINDLESS, binding = DOE_BINDLESS_BINDING_TEXTURES) uniform texture2D u_Textures[kMaxTextures];

void main()
{
    uint textureIndex = min(v_TexIndex, kMaxTextures - 1u);
    vec4 texColor = texture(sampler2D(u_Textures[textureIndex], u_TextureSampler), v_UV);
    vec4 color = texColor * v_Color;
    o_Color = color;
}
