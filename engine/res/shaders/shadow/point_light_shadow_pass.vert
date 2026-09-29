#version 450 core

#include "common/shader_parameter_sets.glsl"

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Normal;
layout(location = 2) in vec2 a_UV;
layout(location = 3) in vec4 a_Model0;
layout(location = 4) in vec4 a_Model1;
layout(location = 5) in vec4 a_Model2;
layout(location = 6) in vec4 a_Model3;
layout(location = 7) in uint a_SkinningOffset;
layout(location = 8) in uvec4 a_BoneIds;
layout(location = 9) in vec4 a_BoneWeights;

layout(location = 0) out vec3 out_position_world_space;

layout(std430, set = DOE_SET_VIEW, binding = DOE_VIEW_BINDING_SKINNING) readonly buffer SkinningBuffer {
    mat4 u_SkinningMatrices[];
};

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
    out_position_world_space = (model * vec4((skinning * vec4(a_Position, 1.0)).xyz, 1.0)).xyz;
}
