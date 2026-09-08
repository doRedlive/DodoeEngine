#version 450 core

#include "shader_parameter_sets.glsl"

layout(location = 0) out vec4 o_Albedo;
layout(location = 1) out vec4 o_Normal;
layout(location = 2) out vec4 o_Position;
layout(location = 3) out vec4 o_Material;
layout(location = 4) out vec4 o_Emissive;

layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec2 v_UV;
layout(location = 2) in vec3 v_WorldPosition;
layout(location = 3) flat in uint v_TexIndex;
layout(location = 4) in vec4 v_ColorTint;
layout(location = 5) flat in uint v_Selected;

layout(set = DOE_SET_GLOBAL, binding = DOE_GLOBAL_BINDING_CONSTANTS) uniform GlobalConstants {
    vec4 u_TimeData;
};
layout(set = DOE_SET_VIEW, binding = DOE_VIEW_BINDING_CONSTANTS) uniform ViewConstants {
    mat4 u_ViewProjection;
};
layout(set = DOE_SET_PRIMITIVE, binding = DOE_PRIMITIVE_BINDING_CONSTANTS) uniform PrimitiveConstants {
    ivec4 u_DrawData;
    vec4 u_MaterialData;
    vec4 u_EmissiveData;
};

const uint kMaxTextures = 1024u;
layout(set = DOE_SET_MATERIAL, binding = DOE_MATERIAL_BINDING_SAMPLER) uniform sampler u_TextureSampler;
layout(set = DOE_SET_BINDLESS, binding = DOE_BINDLESS_BINDING_TEXTURES) uniform texture2D u_Textures[kMaxTextures];

vec3 perturbNormal(vec3 surf_position, vec3 surf_normal, vec2 uv, vec3 tangent_space_normal)
{
    vec3 sigma_x = dFdx(surf_position);
    vec3 sigma_y = dFdy(surf_position);
    vec3 r1 = cross(sigma_y, surf_normal);
    vec3 r2 = cross(surf_normal, sigma_x);
    float det = dot(sigma_x, r1);
    float face = gl_FrontFacing ? 1.0 : -1.0;
    det *= face;
    vec2 slope = tangent_space_normal.xy / max(tangent_space_normal.z, 0.1);
    vec3 gradient = sign(det) * (slope.x * r1 + slope.y * r2);
    return normalize(abs(det) * surf_normal - gradient);
}

void main()
{
    vec3 n = normalize(v_Normal);
    if (u_DrawData.w >= 0) {
        vec3 tangent_space = texture(sampler2D(u_Textures[uint(u_DrawData.w)], u_TextureSampler), v_UV).xyz;
        bool fallback_white = tangent_space.r >= 0.999 && tangent_space.g >= 0.999 && tangent_space.b >= 0.999;
        if (!fallback_white) {
            n = perturbNormal(v_WorldPosition, n, v_UV, tangent_space * 2.0 - 1.0);
        }
    }
    vec3 albedo = texture(sampler2D(u_Textures[v_TexIndex], u_TextureSampler), v_UV).rgb;
    albedo *= v_ColorTint.rgb;
    float metallic = clamp(u_MaterialData.x, 0.0, 1.0);
    float roughness = clamp(u_MaterialData.y, 0.04, 1.0);
    float ao = clamp(u_MaterialData.z, 0.0, 1.0);
    if (u_DrawData.z != 0) {
        vec4 mr_ao = texture(sampler2D(u_Textures[uint(u_DrawData.y)], u_TextureSampler), v_UV);
        metallic = clamp(metallic * mr_ao.b, 0.0, 1.0);
        roughness = clamp(roughness * mr_ao.g, 0.04, 1.0);
        ao = clamp(ao * mr_ao.r, 0.0, 1.0);
    }
    vec3 emissive = u_EmissiveData.rgb;
    if (u_EmissiveData.w > 0.0) {
        vec3 emissive_tex = texture(sampler2D(u_Textures[uint(u_EmissiveData.w) - 1u], u_TextureSampler), v_UV).rgb;
        emissive *= emissive_tex;
    }

    o_Albedo = vec4(albedo, 1.0);
    o_Normal = vec4(n, 1.0);
    o_Position = vec4(v_WorldPosition, 1.0);
    o_Material = vec4(metallic, roughness, ao, float(v_Selected));
    o_Emissive = vec4(max(emissive, vec3(0.0)), 1.0);
}
