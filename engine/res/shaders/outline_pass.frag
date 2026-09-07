#version 450 core

// Selection outline: the GBuffer material alpha channel carries the selection
// mask (1.0 on the selected object). We dilate the mask around the current
// pixel; outline pixels lie in the dilated ring outside the object silhouette.
// Rendered with alpha blending, so non-outline pixels leave the HDR scene
// color untouched.

#include "shader_parameter_sets.glsl"

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT0) uniform texture2D u_SelectionMask;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_SAMPLER) uniform sampler u_Sampler;

float sampleMask(vec2 uv) {
    return texture(sampler2D(u_SelectionMask, u_Sampler), uv).a;
}

bool isOutline(vec2 uv) {
    vec2 texel = 1.0 / vec2(textureSize(sampler2D(u_SelectionMask, u_Sampler), 0));
    const int kDirections = 12;
    const float kRadius = 3.0;
    for (int i = 0; i < kDirections; ++i) {
        float angle = 6.2831853 * float(i) / float(kDirections);
        vec2 offset = vec2(cos(angle), sin(angle)) * texel * kRadius;
        if (sampleMask(uv + offset) > 0.5) {
            return true;
        }
    }
    return false;
}

void main()
{
    if (sampleMask(v_UV) > 0.5) {
        // Inside the selected object: no outline contribution.
        o_Color = vec4(0.0);
        return;
    }
    if (!isOutline(v_UV)) {
        o_Color = vec4(0.0);
        return;
    }
    o_Color = vec4(0.35, 0.70, 1.00, 1.0);
}
