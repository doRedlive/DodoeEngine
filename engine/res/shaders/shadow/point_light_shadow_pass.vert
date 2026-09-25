#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Normal;
layout(location = 2) in vec2 a_UV;
layout(location = 3) in vec4 a_Model0;
layout(location = 4) in vec4 a_Model1;
layout(location = 5) in vec4 a_Model2;
layout(location = 6) in vec4 a_Model3;

layout(location = 0) out vec3 out_position_world_space;

void main()
{
    mat4 model = mat4(a_Model0, a_Model1, a_Model2, a_Model3);
    out_position_world_space = (model * vec4(a_Position, 1.0)).xyz;
}