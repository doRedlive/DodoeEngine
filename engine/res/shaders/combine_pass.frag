#version 450

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

layout(push_constant) uniform CombinePushConstants {
    vec2 u_ViewportPos;
    vec2 u_ViewportSize;
};

layout(set = 0, binding = 0) uniform texture2D u_SceneTexture;
layout(set = 0, binding = 1) uniform texture2D u_ImGuiTexture;
layout(set = 0, binding = 128) uniform sampler u_TextureSampler;

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
        
        // offscreen rendered texture Y inversion in Vulkan
        scene_uv.y = 1.0 - scene_uv.y;
        
        vec4 scene_color = texture(sampler2D(u_SceneTexture, u_TextureSampler), clamp(scene_uv, vec2(0.0), vec2(1.0)));

        bool viewport_mask = imgui_color.r > 0.99 && imgui_color.g < 0.01 && imgui_color.b > 0.99 && imgui_color.a > 0.99;
        if (viewport_mask)
        {
            o_Color = scene_color;
        }
        else
        {
            float overlay_alpha = clamp(imgui_color.a, 0.0, 1.0);
            o_Color = mix(scene_color, imgui_color, overlay_alpha);
        }
    }
    else
    {
        o_Color = imgui_color;
    }
}
