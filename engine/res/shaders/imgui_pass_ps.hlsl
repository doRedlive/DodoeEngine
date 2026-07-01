Texture2D    uTexture : register(t0);
SamplerState uSampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 v_UV     : a_UV;
    float4 v_Color  : a_Color;
};

float4 main(PSInput input) : SV_Target
{
    float4 sampled_color = uTexture.Sample(uSampler, input.v_UV);
    return input.v_Color * sampled_color;
}
