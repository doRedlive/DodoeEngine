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

    float4 i_Transform        : TEXCOORD1;
    float4 i_RotationTexIndex : TEXCOORD2;
    float4 i_UVRect           : TEXCOORD3;
    float4 i_ColorData        : TEXCOORD4;
};

struct VSOutput
{
    float4 Position   : SV_Position;
    float2 v_UV       : TEXCOORD;
    float4 v_Color    : COLOR;
    uint   v_TexIndex : TEXINDEX;
};

float4 unpackRGBA8(uint packed)
{
    return float4(
        float((packed >> 0)  & 0xFF) / 255.0,
        float((packed >> 8)  & 0xFF) / 255.0,
        float((packed >> 16) & 0xFF) / 255.0,
        float((packed >> 24) & 0xFF) / 255.0
    );
}

VSOutput main(VSInput input)
{
    float2 position = input.i_Transform.xy;
    float2 scale    = input.i_Transform.zw;
    float  rotation = input.i_RotationTexIndex.x;
    uint   texIndex = asuint(input.i_RotationTexIndex.z);
    float2 uv_min   = input.i_UVRect.xy;
    float2 uv_max   = input.i_UVRect.zw;
    uint   packedColor = asuint(input.i_ColorData.x);

    float c = cos(rotation);
    float s = sin(rotation);
    float2x2 rotMat = { c, -s, s, c };

    float2 localPos = input.a_Position.xy;
    float2 worldPos = mul(rotMat, localPos * scale) + position;

    VSOutput output;
    output.Position   = mul(u_ViewProjection, float4(worldPos, 0.0, 1.0));
    output.v_UV       = float2(lerp(uv_min.x, uv_max.x, input.a_UV.x),
                               lerp(uv_min.y, uv_max.y, input.a_UV.y));
    output.v_Color    = unpackRGBA8(packedColor);
    output.v_TexIndex = texIndex;
    return output;
}
