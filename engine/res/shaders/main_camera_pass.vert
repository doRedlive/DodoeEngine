#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Normal;
layout(location = 2) in vec2 a_UV;
layout(location = 3) in vec4 a_Model0;
layout(location = 4) in vec4 a_Model1;
layout(location = 5) in vec4 a_Model2;
layout(location = 6) in vec4 a_Model3;
layout(location = 7) in vec4 a_InstanceColorTint;
layout(location = 8) in vec4 a_InstanceParams;

layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec2 v_UV;
layout(location = 2) out vec3 v_WorldPosition;
layout(location = 3) flat out uint v_TexIndex;
layout(location = 4) out vec4 v_ColorTint;

layout(set = 0, binding = 0) uniform MainCameraPassUBO {
    mat4 u_ViewProjection;
    ivec4 u_DrawData;
    vec4 u_MaterialData;
    vec4 u_TimeData;
};

vec3 applyFoliageWind(vec3 local_position, vec4 instance_params)
{
    if (instance_params.w <= 0.5) {
        return local_position;
    }

    float height_weight = clamp(local_position.y, 0.0, 1.0);
    float time = u_TimeData.x;
    float wind_phase = instance_params.x;
    float variation = instance_params.y;
    float bend_strength = instance_params.z;
    float sway = sin(time * (1.5 + variation * 0.35) + wind_phase + local_position.x * 0.2 + local_position.z * 0.15);
    local_position.x += sway * 0.08 * bend_strength * height_weight;
    local_position.z += sway * 0.04 * bend_strength * height_weight;
    return local_position;
}

void main()
{
    mat4 model = mat4(a_Model0, a_Model1, a_Model2, a_Model3);
    mat3 normal_matrix = transpose(inverse(mat3(model)));
    vec3 local_position = applyFoliageWind(a_Position, a_InstanceParams);
    vec4 world_position = model * vec4(local_position, 1.0);
    v_Normal = normalize(normal_matrix * a_Normal.xyz);
    v_UV = a_UV;
    v_WorldPosition = world_position.xyz;
    v_TexIndex = uint(u_DrawData.x);
    v_ColorTint = a_InstanceColorTint;
    gl_Position = u_ViewProjection * world_position;
}
