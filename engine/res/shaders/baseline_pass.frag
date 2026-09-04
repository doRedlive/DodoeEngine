#version 450

#include "shader_parameter_sets.glsl"

layout(location = 0) in vec2 v_UV;

layout(location = 0) out vec4 o_Color;

layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT0) uniform texture2D uTexture;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_SAMPLER) uniform sampler uSampler;

void main()
{
    o_Color = texture(sampler2D(uTexture, uSampler), v_UV);
}
