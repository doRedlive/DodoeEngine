#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_UV;
layout(location = 3) in uint a_TexIndex;

layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec2 v_UV;
layout(location = 2) flat out uint v_TexIndex;

layout(set = 0, binding = 256) uniform MainCameraPassUBO {
    mat4 u_MVP;
};

void main()
{
    v_Normal = a_Normal;
    v_UV = a_UV;
    v_TexIndex = a_TexIndex;
    gl_Position = u_MVP * vec4(a_Position, 1.0);
}
