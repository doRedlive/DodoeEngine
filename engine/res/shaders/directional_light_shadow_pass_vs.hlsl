cbuffer DirectionalLightShadowPassUBO : register(b0)
{
    float4x4 u_LightViewProjection;
    float4   u_TimeData;
};

struct VSInput
{
    float3 a_Position          : a_Position;
    float4 a_Normal            : a_Normal;
    float2 a_UV                : a_UV;
    float4 a_Model0            : TEXCOORD3;
    float4 a_Model1            : TEXCOORD4;
    float4 a_Model2            : TEXCOORD5;
    float4 a_Model3            : TEXCOORD6;
    float4 a_InstanceColorTint : a_InstanceColorTint;
    float4 a_InstanceParams    : a_InstanceParams;
};

float3 applyFoliageWind(float3 local_position, float4 instance_params)
{
    if (instance_params.w <= 0.5)
        return local_position;

    float height_weight = clamp(local_position.y, 0.0, 1.0);
    float t = u_TimeData.x;
    float wind_phase = instance_params.x;
    float variation = instance_params.y;
    float bend_strength = instance_params.z;
    float sway = sin(t * (1.5 + variation * 0.35) + wind_phase + local_position.x * 0.2 + local_position.z * 0.15);
    local_position.x += sway * 0.08 * bend_strength * height_weight;
    local_position.z += sway * 0.04 * bend_strength * height_weight;
    return local_position;
}

float4 main(VSInput input) : SV_Position
{
    float4x4 model = float4x4(input.a_Model0, input.a_Model1, input.a_Model2, input.a_Model3);
    float3 local_position = applyFoliageWind(input.a_Position, input.a_InstanceParams);
    float4 world_position = mul(model, float4(local_position, 1.0));
    return mul(u_LightViewProjection, world_position);
}
