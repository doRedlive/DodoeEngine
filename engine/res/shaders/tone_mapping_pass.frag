#version 450

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

layout(set = 0, binding = 0) uniform texture2D u_InputTexture;
layout(set = 0, binding = 128) uniform sampler u_Sampler;

vec3 TonemapACES(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdr_color = texture(sampler2D(u_InputTexture, u_Sampler), v_UV).rgb;
    o_Color = vec4(TonemapACES(hdr_color), 1.0);
}