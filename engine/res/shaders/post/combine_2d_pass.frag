#version 450

#include "common/shader_parameter_sets.glsl"

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_CONSTANTS) uniform PresentViewport {
    vec2 u_ViewportPos;
    vec2 u_ViewportSize;
};
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT0) uniform texture2D u_SceneTexture;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT1) uniform texture2D u_ImGuiTexture;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_SAMPLER) uniform sampler u_TextureSampler;

void main()
{
    // Both inputs use framebuffer coordinates. v_UV is local to the output viewport.
    vec2 size = vec2(textureSize(sampler2D(u_SceneTexture, u_TextureSampler), 0));
    vec2 uv = (u_ViewportPos + v_UV * u_ViewportSize) / max(size, vec2(1.0));
    vec4 scene = texture(sampler2D(u_SceneTexture, u_TextureSampler), uv);
    vec4 overlay = texture(sampler2D(u_ImGuiTexture, u_TextureSampler), uv);
    o_Color = overlay + scene * (1.0 - overlay.a);
}
