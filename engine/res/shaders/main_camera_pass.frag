#version 450 core

layout(location = 0) out vec4 o_Color;
layout(location = 0) in vec3 v_Normal;

void main()
{
    vec3 n = normalize(v_Normal);
    vec3 lit = vec3(0.35, 0.55, 0.85) * (0.4 + 0.6 * max(dot(n, normalize(vec3(0.3, 0.8, 0.5))), 0.0));
    o_Color = vec4(lit, 1.0);
}
