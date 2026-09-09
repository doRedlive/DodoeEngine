#version 450 core

#include "shader_parameter_sets.glsl"

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Normal;
layout(location = 2) in vec2 a_UV;
layout(location = 3) in vec4 a_Model0;
layout(location = 4) in vec4 a_Model1;
layout(location = 5) in vec4 a_Model2;
layout(location = 6) in vec4 a_Model3;
layout(location = 7) in vec4 a_InstanceColorTint;
layout(location = 8) in vec4 a_InstanceParams;
layout(location = 9) in vec4 a_PrevModel0;
layout(location = 10) in vec4 a_PrevModel1;
layout(location = 11) in vec4 a_PrevModel2;
layout(location = 12) in vec4 a_PrevModel3;

layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec2 v_UV;
layout(location = 2) out vec3 v_WorldPosition;
layout(location = 3) flat out uint v_TexIndex;
layout(location = 4) out vec4 v_ColorTint;
layout(location = 5) flat out uint v_Selected;
layout(location = 6) out vec4 v_CurrClip;
layout(location = 7) out vec4 v_PrevClip;

layout(set = DOE_SET_GLOBAL, binding = DOE_GLOBAL_BINDING_CONSTANTS) uniform GlobalConstants {
    vec4 u_TimeData;
};
layout(set = DOE_SET_VIEW, binding = DOE_VIEW_BINDING_CONSTANTS) uniform ViewConstants {
    mat4 u_ViewProjection;
    mat4 u_PrevViewProjection;
    vec4 u_PrevJitterUV;
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
    mat4 model = mat4(a_Model0, a_Model1, a_Model2, a_Model3);
    mat4 prev_model = mat4(a_PrevModel0, a_PrevModel1, a_PrevModel2, a_PrevModel3);
    mat3 normal_matrix = transpose(inverse(mat3(model)));
    vec3 local_position = applyFoliageWind(a_Position, a_InstanceParams);
    vec4 world_position = model * vec4(local_position, 1.0);
    v_Normal = normalize(normal_matrix * a_Normal.xyz);
    v_UV = a_UV;
    v_WorldPosition = world_position.xyz;
    v_TexIndex = uint(u_DrawData.x);
    v_ColorTint = a_InstanceColorTint;
    // Selection flag travels in the (otherwise unused) tint alpha channel.
    v_Selected = a_InstanceColorTint.a < 0.5 ? 1u : 0u;
    vec4 curr_clip = u_ViewProjection * world_position;
    vec4 prev_clip = vec4(0.0);
    if (curr_clip.w > 0.0001) {
        vec4 prev_world = prev_model * vec4(a_Position, 1.0);
        prev_clip = u_PrevViewProjection * prev_world;
    }
    v_CurrClip = curr_clip;
    v_PrevClip = prev_clip;
    gl_Position = curr_clip;
}
