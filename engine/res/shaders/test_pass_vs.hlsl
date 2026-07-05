struct VSInput
{
    float3 a_Position : POSITION;
    float4 a_Color    : COLOR;
    float2 a_UV       : TEXCOORD;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float4 v_Color  : COLOR;
    float2 v_UV     : TEXCOORD;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Position = float4(input.a_Position, 1.0);
    output.v_Color  = input.a_Color;
    output.v_UV     = input.a_UV;
    return output;
}
