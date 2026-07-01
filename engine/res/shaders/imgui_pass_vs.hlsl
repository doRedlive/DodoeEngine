cbuffer PushConstants : register(b0)
{
    float2 uInvDisplaySize;
    float2 uDisplayPos;
};

struct VSInput
{
    float2 a_Position : a_Position;
    float2 a_UV       : a_UV;
    float4 a_Color    : a_Color;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float2 v_UV     : a_UV;
    float4 v_Color  : a_Color;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.v_UV    = input.a_UV;
    output.v_Color = input.a_Color;

    float2 pos = input.a_Position - uDisplayPos;
    float2 ndc;
    ndc.x = pos.x * uInvDisplaySize.x * 2.0 - 1.0;
    ndc.y = 1.0 - pos.y * uInvDisplaySize.y * 2.0;
    output.Position = float4(ndc, 0.0, 1.0);
    return output;
}
