// do@Redlive
// Editor Gizmo vertex shader - unlit colored lines/triangles

struct VSInput
{
    float3 a_Position : POSITION;
    float4 a_Color    : COLOR;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float4 v_Color  : COLOR;
};

cbuffer GizmoConstants : register(b0)
{
    float4x4 u_MVP;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Position = mul(u_MVP, float4(input.a_Position, 1.0));
    output.v_Color  = input.a_Color;
    return output;
}
