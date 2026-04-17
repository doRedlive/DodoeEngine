#version 450 core

layout(location = 0) out vec4 o_Color;
layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec2 v_UV;
layout(location = 2) flat in uint v_TexIndex;

const uint kMaxTextures = 1024u;
layout(set = 0, binding = 128) uniform sampler u_TextureSampler;
layout(set = 1, binding = 0) uniform texture2D u_Textures[kMaxTextures];

void main()
{
    vec3 n = normalize(v_Normal);
    float ndotl = max(dot(n, normalize(vec3(0.3, 0.8, 0.5))), 0.0);
    vec3 lighting = vec3(0.35 + 0.65 * ndotl);
    vec3 albedo = texture(sampler2D(u_Textures[v_TexIndex], u_TextureSampler), v_UV).rgb;
    o_Color = vec4(albedo * lighting, 1.0);
}
