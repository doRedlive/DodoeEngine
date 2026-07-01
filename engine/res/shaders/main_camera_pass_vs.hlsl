cbuffer MainCameraPassUBO : register(b0)
{
    float4x4 u_ViewProjection;
    int4     u_DrawData;
    float4   u_MaterialData;
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

struct VSOutput
{
    float4 Position        : SV_Position;
    float3 v_Normal        : a_Normal;
    float2 v_UV            : a_UV;
    float3 v_WorldPosition : TEXCOORD0;
    uint   v_TexIndex      : TEXCOORD1;
    float4 v_ColorTint     : COLOR0;
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

VSOutput main(VSInput input)
{
    float4x4 model = float4x4(input.a_Model0, input.a_Model1, input.a_Model2, input.a_Model3);
    float3x3 m = (float3x3)model;
    float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
              - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
              + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    float3x3 normal_matrix = (float3x3)0;
    if (abs(det) > 1e-10)
    {
        float inv = 1.0 / det;
        normal_matrix[0][0] =  (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * inv;
        normal_matrix[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) * inv;
        normal_matrix[0][2] =  (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * inv;
        normal_matrix[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) * inv;
        normal_matrix[1][1] =  (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * inv;
        normal_matrix[1][2] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]) * inv;
        normal_matrix[2][0] =  (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * inv;
        normal_matrix[2][1] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]) * inv;
        normal_matrix[2][2] =  (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * inv;
    }
    float3 local_position = applyFoliageWind(input.a_Position, input.a_InstanceParams);
    float4 world_position = mul(model, float4(local_position, 1.0));

    VSOutput output;
    output.Position        = mul(u_ViewProjection, world_position);
    output.v_Normal        = normalize(mul(normal_matrix, input.a_Normal.xyz));
    output.v_UV            = input.a_UV;
    output.v_WorldPosition = world_position.xyz;
    output.v_TexIndex      = (uint)u_DrawData.x;
    output.v_ColorTint     = input.a_InstanceColorTint;
    return output;
}
