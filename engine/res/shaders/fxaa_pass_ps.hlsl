Texture2D    u_InputTexture : register(t0);
SamplerState u_Sampler      : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 v_UV     : TEXCOORD0;
};

float3 SampleColor(float2 uv) { return u_InputTexture.Sample(u_Sampler, uv).rgb; }

float4 main(PSInput input) : SV_Target
{
    uint2 texSize; u_InputTexture.GetDimensions(texSize.x, texSize.y);
    float2 texel_size = 1.0 / float2(float(texSize.x), float(texSize.y));

    float3 rgb_nw = SampleColor(input.v_UV + texel_size * float2(-1.0, -1.0));
    float3 rgb_ne = SampleColor(input.v_UV + texel_size * float2( 1.0, -1.0));
    float3 rgb_sw = SampleColor(input.v_UV + texel_size * float2(-1.0,  1.0));
    float3 rgb_se = SampleColor(input.v_UV + texel_size * float2( 1.0,  1.0));
    float3 rgb_m  = SampleColor(input.v_UV);

    float3 luma = float3(0.299, 0.587, 0.114);
    float luma_nw = dot(rgb_nw, luma), luma_ne = dot(rgb_ne, luma);
    float luma_sw = dot(rgb_sw, luma), luma_se = dot(rgb_se, luma);
    float luma_m  = dot(rgb_m,  luma);

    float luma_min = min(luma_m, min(min(luma_nw, luma_ne), min(luma_sw, luma_se)));
    float luma_max = max(luma_m, max(max(luma_nw, luma_ne), max(luma_sw, luma_se)));

    float2 dir = float2(-((luma_nw + luma_ne) - (luma_sw + luma_se)),
                         ((luma_nw + luma_sw) - (luma_ne + luma_se)));
    float dir_reduce = max((luma_nw + luma_ne + luma_sw + luma_se) * 0.25 * 0.5, 1.0 / 128.0);
    float rcp_dir_min = 1.0 / (min(abs(dir.x), abs(dir.y)) + dir_reduce);
    dir = clamp(dir * rcp_dir_min * texel_size, float2(-8.0, -8.0), float2(8.0, 8.0));

    float3 rgb_a = 0.5 * (SampleColor(input.v_UV + dir * (1.0 / 3.0 - 0.5)) +
                          SampleColor(input.v_UV + dir * (2.0 / 3.0 - 0.5)));
    float3 rgb_b = rgb_a * 0.5 + 0.25 * (SampleColor(input.v_UV + dir * -0.5) +
                                          SampleColor(input.v_UV + dir *  0.5));

    float luma_b = dot(rgb_b, luma);
    if (luma_b < luma_min || luma_b > luma_max)
        return float4(rgb_a, 1.0);
    else
        return float4(rgb_b, 1.0);
}
