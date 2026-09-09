#version 450

#include "shader_parameter_sets.glsl"

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Depth;

layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT0) uniform texture2D u_Depth;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_SAMPLER) uniform sampler u_Sampler;

void main()
{
    o_Depth = vec4(texture(sampler2D(u_Depth, u_Sampler), v_UV).x, 0.0, 0.0, 1.0);
}
