#version 450

// Non-bindless sprite pixel shader — single texture slot fallback

layout(set = 2, binding = 0) uniform texture2D u_Texture;
layout(set = 1, binding = 0) uniform sampler u_TextureSampler;

layout(location = 0) in vec2 v_UV;
layout(location = 1) in vec4 v_Color;
layout(location = 2) flat in uint v_TexIndex;

layout(location = 0) out vec4 o_Color;

void main()
{
    vec4 texColor = texture(sampler2D(u_Texture, u_TextureSampler), v_UV);
    o_Color = texColor * v_Color;
}
