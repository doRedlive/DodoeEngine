#version 450

#include "shader_parameter_sets.glsl"

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

layout(push_constant) uniform CombinePushConstants {
    vec2 u_ViewportPos;
    vec2 u_ViewportSize;
};

layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT0) uniform texture2D u_SceneTexture;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT1) uniform texture2D u_ImGuiTexture;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_SAMPLER) uniform sampler u_TextureSampler;

void main()
{
    vec4 imgui_color = texture(sampler2D(u_ImGuiTexture, u_TextureSampler), v_UV);
    vec2 framebuffer_size = vec2(textureSize(sampler2D(u_ImGuiTexture, u_TextureSampler), 0));
    vec2 viewport_max = u_ViewportPos + u_ViewportSize;
    vec2 frag_pos = v_UV * framebuffer_size;

    bool inside_viewport = frag_pos.x >= u_ViewportPos.x && frag_pos.y >= u_ViewportPos.y &&
                           frag_pos.x <= viewport_max.x && frag_pos.y <= viewport_max.y;

    if (inside_viewport)
    {
        vec2 safe_viewport_size = max(u_ViewportSize, vec2(1.0));
        vec2 scene_uv = (frag_pos - u_ViewportPos) / safe_viewport_size;
        scene_uv.y = 1.0 - scene_uv.y;
        vec4 scene_color = texture(sampler2D(u_SceneTexture, u_TextureSampler), clamp(scene_uv, vec2(0.0), vec2(1.0)));
        o_Color = mix(scene_color, imgui_color, imgui_color.a);
    }
    else
    {
        o_Color = imgui_color;
    }
}
