cbuffer SpriteCameraUBO : register(b0)
{
    float4x4 u_ViewProjection;
};

struct VSInput
{
    float3 a_Position : POSITION;
    float2 a_UV       : TEXCOORD;
    float4 a_Color    : COLOR;
    uint   a_TexIndex : TEXINDEX;
};

struct VSOutput
{
    float4 Position   : SV_Position;
    float2 v_UV       : TEXCOORD;
    float4 v_Color    : COLOR;
    uint   v_TexIndex : TEXINDEX;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Position   = mul(u_ViewProjection, float4(input.a_Position, 1.0));
    output.v_UV       = input.a_UV;
    output.v_Color    = input.a_Color;
    output.v_TexIndex = input.a_TexIndex;
    return output;
}
