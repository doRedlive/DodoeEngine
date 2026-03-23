#version 450 core

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec2  a_UV;
layout(location = 2) in vec4  a_Color;
layout(location = 3) in float a_TexIndex;

out vec2 v_UV;
out vec4 v_Color;
flat out float v_TexIndex;

uniform mat4 u_ViewProj;

void main() 
{
    v_UV = a_UV;
    v_Color = a_Color;
    v_TexIndex = a_TexIndex;

    gl_Position = u_ViewProj * vec4(a_Position, 1.0);
}
