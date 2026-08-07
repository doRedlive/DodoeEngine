// do@Redlive

cbuffer UIVP : register(b0)
{
    float4x4 u_ViewProjection;
};

struct VSInput
{
    float3 a_Position : POSITION;
    float2 a_UV       : TEXCOORD;
    float4 a_Color    : COLOR;
    uint   a_TexIndex : TEXINDEX;

    float4 i_Transform  : TEXCOORD1;  // position.xy, size.xy
    float4 i_UVRect     : TEXCOORD2;  // uv_min.xy, uv_max.xy
    float4 i_ColorDepth : TEXCOORD3;  // color, atlas_index, depth, flags
    float4 i_ClipRect   : TEXCOORD4;  // clip_rect (not used in VS)
};

struct VSOutput
{
    float4 Position   : SV_Position;
    float2 v_UV       : TEXCOORD0;
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
    float2 position   = input.i_Transform.xy;
    float2 size       = input.i_Transform.zw;
    float2 uv_min     = input.i_UVRect.xy;
    float2 uv_max     = input.i_UVRect.zw;
    uint   packedColor = asuint(input.i_ColorDepth.x);
    uint   texIndex    = asuint(input.i_ColorDepth.y);
    float  depth       = input.i_ColorDepth.z;

    // Scale quad from [-0.5, 0.5] to pixel-size, position in screen space
    float2 worldPos = input.a_Position.xy * size + position;

    VSOutput output;
    output.Position   = mul(u_ViewProjection, float4(worldPos, depth, 1.0));
    output.v_UV       = float2(lerp(uv_min.x, uv_max.x, input.a_UV.x),
                               lerp(uv_min.y, uv_max.y, input.a_UV.y));
    output.v_Color    = unpackRGBA8(packedColor);
    output.v_TexIndex = texIndex;
    return output;
}
