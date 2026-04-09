#version 450 core

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec2  a_UV;
layout(location = 2) in vec4  a_Color;
layout(location = 3) in float a_TexIndex;

layout(location = 0) out vec2 v_UV;
layout(location = 1) out vec4 v_Color;
layout(location = 2) flat out float v_TexIndex;

void main() 
{
    v_UV = a_UV;
    v_Color = a_Color;
    v_TexIndex = a_TexIndex;

    gl_Position = vec4(a_Position, 1.0);
}
