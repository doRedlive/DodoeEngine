#version 450 core

#include "shader_parameter_sets.glsl"

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Normal;
layout(location = 2) in vec2 a_UV;
layout(location = 3) in vec4 a_Model0;
layout(location = 4) in vec4 a_Model1;
layout(location = 5) in vec4 a_Model2;
layout(location = 6) in vec4 a_Model3;

layout(location = 0) flat out uint v_NodeId;

layout(set = DOE_SET_VIEW, binding = DOE_VIEW_BINDING_CONSTANTS) uniform PickPassUBO {
    mat4 u_ViewProjection;
};
layout(set = DOE_SET_PRIMITIVE, binding = DOE_PRIMITIVE_BINDING_CONSTANTS) uniform PrimitiveConstants {
    ivec4 u_DrawData;
    vec4 u_MaterialData;
};

void main()
{
    mat4 model = mat4(a_Model0, a_Model1, a_Model2, a_Model3);
    vec4 world_position = model * vec4(a_Position, 1.0);
    v_NodeId = uint(u_DrawData.w);
    gl_Position = u_ViewProjection * world_position;
}
