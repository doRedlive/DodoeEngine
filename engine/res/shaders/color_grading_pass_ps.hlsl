cbuffer ColorGradingPushConstants : register(b0)
{
    float4 u_Params; // x=exposure, y=saturation, z=contrast, w=gamma
};

Texture2D    u_InputTexture : register(t0, space2);
SamplerState u_Sampler      : register(s0, space1);

struct PSInput
{
    float4 Position : SV_Position;
    float2 v_UV     : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    float3 color     = u_InputTexture.Sample(u_Sampler, input.v_UV).rgb;
    float exposure   = u_Params.x;
    float saturation = u_Params.y;
    float contrast   = u_Params.z;
    float gamma_val  = max(u_Params.w, 0.0001);

    color *= exposure;
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    color = lerp(float3(luminance, luminance, luminance), color, saturation);
    color = (color - 0.5) * contrast + 0.5;
    color = pow(max(color, float3(0.0, 0.0, 0.0)), float3(1.0 / gamma_val, 1.0 / gamma_val, 1.0 / gamma_val));
    return float4(clamp(color, 0.0, 1.0), 1.0);
}
