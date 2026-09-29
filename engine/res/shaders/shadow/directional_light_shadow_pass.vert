#version 450 core

#include "common/shader_parameter_sets.glsl"

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Normal;
layout(location = 2) in vec2 a_UV;
layout(location = 3) in vec4 a_Model0;
layout(location = 4) in vec4 a_Model1;
layout(location = 5) in vec4 a_Model2;
layout(location = 6) in vec4 a_Model3;
layout(location = 7) in vec4 a_InstanceColorTint;
layout(location = 8) in vec4 a_InstanceParams;
layout(location = 9) in uint a_SkinningOffset;
layout(location = 10) in uvec4 a_BoneIds;
layout(location = 11) in vec4 a_BoneWeights;

layout(set = DOE_SET_GLOBAL, binding = DOE_GLOBAL_BINDING_CONSTANTS) uniform GlobalConstants {
    vec4 u_TimeData;
};
layout(set = DOE_SET_VIEW, binding = DOE_VIEW_BINDING_CONSTANTS) uniform ViewConstants {
    mat4 u_LightViewProjection;
};
layout(std430, set = DOE_SET_VIEW, binding = DOE_VIEW_BINDING_SKINNING) readonly buffer SkinningBuffer {
    mat4 u_SkinningMatrices[];
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

mat4 computeSkinningMatrix()
{
    float total_weight = a_BoneWeights.x + a_BoneWeights.y + a_BoneWeights.z + a_BoneWeights.w;
    if (total_weight <= 0.0 || a_SkinningOffset == 0xFFFFFFFFu) {
        return mat4(1.0);
    }
    mat4 skinning = a_BoneWeights.x * u_SkinningMatrices[a_SkinningOffset + a_BoneIds.x];
    skinning += a_BoneWeights.y * u_SkinningMatrices[a_SkinningOffset + a_BoneIds.y];
    skinning += a_BoneWeights.z * u_SkinningMatrices[a_SkinningOffset + a_BoneIds.z];
    skinning += a_BoneWeights.w * u_SkinningMatrices[a_SkinningOffset + a_BoneIds.w];
    return skinning;
}

void main()
{
    mat4 model = mat4(a_Model0, a_Model1, a_Model2, a_Model3);
    mat4 skinning = computeSkinningMatrix();
    vec3 local_position = (skinning * vec4(applyFoliageWind(a_Position, a_InstanceParams), 1.0)).xyz;
    vec4 world_position = model * vec4(local_position, 1.0);
    gl_Position = u_LightViewProjection * world_position;
}
