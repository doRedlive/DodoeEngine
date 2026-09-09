#version 450 core

#include "shader_parameter_sets.glsl"

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Normal;
layout(location = 2) in vec2 a_UV;
layout(location = 3) in vec4 a_InstanceColorTint;
layout(location = 4) in vec4 a_InstanceParams;
layout(location = 5) in uint a_TransformIndex;
layout(location = 6) in ivec4 a_DrawData;

layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec2 v_UV;
layout(location = 2) out vec3 v_WorldPosition;
layout(location = 3) flat out uint v_TexIndex;
layout(location = 4) out vec4 v_ColorTint;
layout(location = 5) flat out uint v_Selected;
layout(location = 6) out vec4 v_CurrClip;
layout(location = 7) out vec4 v_PrevClip;

struct GpuTransform
{
    mat4 local_to_world;
    mat4 world_to_local;
};

layout(set = DOE_SET_GLOBAL, binding = DOE_GLOBAL_BINDING_CONSTANTS) uniform GlobalConstants {
    vec4 u_TimeData;
};
layout(set = DOE_SET_VIEW, binding = DOE_VIEW_BINDING_CONSTANTS) uniform ViewConstants {
    mat4 u_ViewProjection;
};
layout(std430, set = DOE_SET_VIEW, binding = DOE_VIEW_BINDING_TRANSFORMS) readonly buffer TransformBuffer
{
    GpuTransform transforms[];
};
layout(set = DOE_SET_PRIMITIVE, binding = DOE_PRIMITIVE_BINDING_CONSTANTS) uniform PrimitiveConstants {
    ivec4 u_DrawData;
    vec4 u_MaterialData;
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
    mat4 model = transforms[a_TransformIndex].local_to_world;
    mat3 normal_matrix = transpose(inverse(mat3(model)));
    vec3 local_position = applyFoliageWind(a_Position, a_InstanceParams);
    vec4 world_position = model * vec4(local_position, 1.0);
    v_Normal = normalize(normal_matrix * a_Normal.xyz);
    v_UV = a_UV;
    v_WorldPosition = world_position.xyz;
    v_TexIndex = uint(a_DrawData.x);
    v_ColorTint = a_InstanceColorTint;
    // Selection flag travels in the (otherwise unused) tint alpha channel.
    v_Selected = a_InstanceColorTint.a < 0.5 ? 1u : 0u;
    v_CurrClip = vec4(0.0, 0.0, 0.0, 1.0);
    v_PrevClip = vec4(0.0, 0.0, 0.0, 1.0);
    gl_Position = u_ViewProjection * world_position;
}
