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

layout(set = 0, binding = 0) uniform DirectionalLightShadowPassUBO {
    mat4 u_LightViewProjection;
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
    vec3 local_position = applyFoliageWind(a_Position, a_InstanceParams);
    vec4 world_position = model * vec4(local_position, 1.0);
    gl_Position = u_LightViewProjection * world_position;
}
