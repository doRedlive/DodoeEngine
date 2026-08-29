#version 450 core

#include "shader_parameter_sets.glsl"

layout(location = 0) out vec4 o_Albedo;
layout(location = 1) out vec4 o_Normal;
layout(location = 2) out vec4 o_Position;
layout(location = 3) out vec4 o_Material;

layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec2 v_UV;
layout(location = 2) in vec3 v_WorldPosition;
layout(location = 3) flat in uint v_TexIndex;
layout(location = 4) in vec4 v_ColorTint;

layout(set = DOE_SET_GLOBAL, binding = DOE_GLOBAL_BINDING_CONSTANTS) uniform GlobalConstants {
    vec4 u_TimeData;
};
layout(set = DOE_SET_VIEW, binding = DOE_VIEW_BINDING_CONSTANTS) uniform ViewConstants {
    mat4 u_ViewProjection;
};
layout(set = DOE_SET_PRIMITIVE, binding = DOE_PRIMITIVE_BINDING_CONSTANTS) uniform PrimitiveConstants {
    ivec4 u_DrawData;
    vec4 u_MaterialData;
};

const uint kMaxTextures = 1024u;
layout(set = DOE_SET_MATERIAL, binding = DOE_MATERIAL_BINDING_SAMPLER) uniform sampler u_TextureSampler;
layout(set = DOE_SET_BINDLESS, binding = DOE_BINDLESS_BINDING_TEXTURES) uniform texture2D u_Textures[kMaxTextures];

void main()
{
    vec3 n = normalize(v_Normal);
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
    
    o_Albedo = vec4(albedo, 1.0);
    o_Normal = vec4(n, 1.0);
    o_Position = vec4(v_WorldPosition, 1.0);
    o_Material = vec4(metallic, roughness, ao, 1.0);
}
