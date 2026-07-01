cbuffer CombinePushConstants : register(b0)
{
    float2 u_ViewportPos;
    float2 u_ViewportSize;
};

Texture2D    u_SceneTexture   : register(t0);
Texture2D    u_ImGuiTexture   : register(t1);
SamplerState u_TextureSampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 v_UV     : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    float4 imgui_color = u_ImGuiTexture.Sample(u_TextureSampler, input.v_UV);

    uint2 texSize; u_ImGuiTexture.GetDimensions(texSize.x, texSize.y);
    float2 framebuffer_size = float2(float(texSize.x), float(texSize.y));
    float2 viewport_max = u_ViewportPos + u_ViewportSize;
    float2 frag_pos = input.v_UV * framebuffer_size;

    bool inside_viewport = frag_pos.x >= u_ViewportPos.x && frag_pos.y >= u_ViewportPos.y &&
                           frag_pos.x <= viewport_max.x && frag_pos.y <= viewport_max.y;

    if (inside_viewport)
    {
        float2 safe_viewport_size = max(u_ViewportSize, float2(1.0, 1.0));
        float2 scene_uv = (frag_pos - u_ViewportPos) / safe_viewport_size;
        scene_uv.y = 1.0 - scene_uv.y;
        float4 scene_color = u_SceneTexture.Sample(u_TextureSampler, clamp(scene_uv, float2(0.0, 0.0), float2(1.0, 1.0)));
        return lerp(scene_color, imgui_color, imgui_color.a);
    }
    else
    {
        return imgui_color;
    }
}
