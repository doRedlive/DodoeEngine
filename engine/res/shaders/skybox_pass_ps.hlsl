cbuffer SkyboxPushConstants : register(b0)
{
    float4x4 u_InvViewProjection;
};

TextureCube  u_SkyboxTexture : register(t0);
Texture2D    u_SceneDepth    : register(t1);
SamplerState u_Sampler       : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 v_UV     : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    float2 ndc = float2(input.v_UV.x * 2.0 - 1.0, 1.0 - input.v_UV.y * 2.0);
    float4 near_world = mul(u_InvViewProjection, float4(ndc, 0.0, 1.0));
    float4 far_world  = mul(u_InvViewProjection, float4(ndc, 1.0, 1.0));
    near_world /= max(abs(near_world.w), 1e-6);
    far_world  /= max(abs(far_world.w),  1e-6);
    float3 view_dir = normalize(far_world.xyz - near_world.xyz);

    float scene_depth = u_SceneDepth.Sample(u_Sampler, input.v_UV).r;
    if (scene_depth < 0.99999)
        discard;

    float3 sky_color = u_SkyboxTexture.Sample(u_Sampler, view_dir).rgb;
    return float4(sky_color, 1.0);
}
