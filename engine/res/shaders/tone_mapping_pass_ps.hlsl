Texture2D    u_InputTexture : register(t0, space2);
SamplerState u_Sampler      : register(s0, space1);

struct PSInput
{
    float4 Position : SV_Position;
    float2 v_UV     : TEXCOORD0;
};

float3 TonemapACES(float3 color)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

float4 main(PSInput input) : SV_Target
{
    float3 hdr_color = u_InputTexture.Sample(u_Sampler, input.v_UV).rgb;
    return float4(TonemapACES(hdr_color), 1.0);
}
