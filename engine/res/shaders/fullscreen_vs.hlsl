struct VSOutput
{
    float4 Position : SV_Position;
    float2 v_UV     : TEXCOORD0;
};

static const float2 NDC[6] =
{
    float2(-1.0, -1.0),
    float2( 1.0, -1.0),
    float2( 1.0,  1.0),
    float2(-1.0, -1.0),
    float2( 1.0,  1.0),
    float2(-1.0,  1.0)
};

VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;
    float2 pos = NDC[vertexID];
    output.Position = float4(pos, 0.0, 1.0);
    output.v_UV     = float2(pos.x * 0.5 + 0.5, 0.5 - pos.y * 0.5);
    return output;
}
