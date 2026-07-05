Texture2D uTexture : register(t0);
SamplerState uSampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float4 v_Color  : COLOR;
    float2 v_UV     : TEXCOORD;
};

float4 main(PSInput input) : SV_Target
{
    float4 texColor = uTexture.Sample(uSampler, input.v_UV);
    return texColor * input.v_Color;
}
