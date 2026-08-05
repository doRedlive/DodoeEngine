SamplerState u_TextureSampler : register(s0, space1);

Texture2D u_Textures[] : register(t0, space3);

struct PSInput
{
    float4 Position   : SV_Position;
    float2 v_UV       : TEXCOORD;
    float4 v_Color    : COLOR;
    uint   v_TexIndex : TEXINDEX;
};

float4 main(PSInput input) : SV_Target
{
    uint textureIndex = min(input.v_TexIndex, 1023u);
    float4 texColor = u_Textures[textureIndex].Sample(u_TextureSampler, input.v_UV);
    return texColor * input.v_Color;
}
