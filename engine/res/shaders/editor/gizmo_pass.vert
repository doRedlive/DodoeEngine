#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout(location = 0) out vec4 v_Color;

layout(push_constant) uniform GizmoConstants {
    mat4 u_MVP;
    vec4 u_Color;
};

void main()
{
    v_Color = a_Color;
    gl_Position = u_MVP * vec4(a_Position, 1.0);
}
