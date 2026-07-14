SamplerState u_TextureSampler : register(s0);

Texture2D u_Texture : register(t0);

struct PSInput
{
    float4 Position   : SV_Position;
    float2 v_UV       : TEXCOORD;
    float4 v_Color    : COLOR;
    uint   v_TexIndex : TEXINDEX;
};

float4 main(PSInput input) : SV_Target
{
    float4 texColor = u_Texture.Sample(u_TextureSampler, input.v_UV);
    return texColor * input.v_Color;
}
