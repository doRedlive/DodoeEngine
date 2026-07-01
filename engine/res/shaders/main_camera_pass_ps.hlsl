cbuffer MainCameraPassUBO : register(b0)
{
    float4x4 u_ViewProjection;
    int4     u_DrawData;
    float4   u_MaterialData;
    float4   u_TimeData;
};

SamplerState u_TextureSampler : register(s0);
Texture2D u_Textures[] : register(t0);

struct PSInput
{
    float4 Position        : SV_Position;
    float3 v_Normal        : a_Normal;
    float2 v_UV            : a_UV;
    float3 v_WorldPosition : TEXCOORD0;
    uint   v_TexIndex      : TEXCOORD1;
    float4 v_ColorTint     : COLOR0;
};

struct PSOutput
{
    float4 o_Albedo   : SV_Target0;
    float4 o_Normal   : SV_Target1;
    float4 o_Position : SV_Target2;
    float4 o_Material : SV_Target3;
};

PSOutput main(PSInput input)
{
    float3 n = normalize(input.v_Normal);
    float3 albedo = u_Textures[input.v_TexIndex].Sample(u_TextureSampler, input.v_UV).rgb;
    albedo *= input.v_ColorTint.rgb;

    float metallic  = clamp(u_MaterialData.x, 0.0, 1.0);
    float roughness = clamp(u_MaterialData.y, 0.04, 1.0);
    float ao        = clamp(u_MaterialData.z, 0.0, 1.0);

    if (u_DrawData.z != 0)
    {
        float4 mr_ao = u_Textures[(uint)u_DrawData.y].Sample(u_TextureSampler, input.v_UV);
        metallic  = clamp(metallic  * mr_ao.b, 0.0, 1.0);
        roughness = clamp(roughness * mr_ao.g, 0.04, 1.0);
        ao        = clamp(ao        * mr_ao.r, 0.0, 1.0);
    }

    PSOutput output;
    output.o_Albedo   = float4(albedo, 1.0);
    output.o_Normal   = float4(n, 1.0);
    output.o_Position = float4(input.v_WorldPosition, 1.0);
    output.o_Material = float4(metallic, roughness, ao, 1.0);
    return output;
}
