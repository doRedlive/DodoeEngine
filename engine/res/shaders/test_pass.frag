#version 450 core

#include "shader_parameter_sets.glsl"

layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_UV;

layout(location = 0) out vec4 o_Color;

layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT0) uniform texture2D uTexture;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_SAMPLER) uniform sampler uSampler;

void main()
{
    vec4 texColor = texture(sampler2D(uTexture, uSampler), v_UV);
    o_Color = texColor * v_Color;
}
