#version 450 core

#include "shader_parameter_sets.glsl"

layout(location = 0) out vec4 o_Color;

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
    vec4 u_EmissiveData;
};
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_CONSTANTS) uniform OpaquePassUBO {
    vec4 u_CameraPosition;
    vec4 u_DirectionalColorIntensity;
    vec4 u_DirectionalDirectionFlags;
    vec4 u_CameraDirection;
    mat4 u_CascadeViewProjections[4];
    vec4 u_CascadeSplits;
    vec4 u_ShadowParams;
    vec4 u_PointLightColors[4];
    vec4 u_PointLightPositions[4];
    vec4 u_LightCountFlags;
    vec4 u_IrradianceSH[9];
    vec4 u_IblParams;
};
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT0) uniform texture2D u_ShadowMap;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT1) uniform textureCube u_SkyboxTexture;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT2) uniform texture2D u_BrdfLut;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_SAMPLER) uniform sampler u_Sampler;

const uint kMaxTextures = 1024u;
layout(set = DOE_SET_MATERIAL, binding = DOE_MATERIAL_BINDING_SAMPLER) uniform sampler u_TextureSampler;
layout(set = DOE_SET_BINDLESS, binding = DOE_BINDLESS_BINDING_TEXTURES) uniform texture2D u_Textures[kMaxTextures];

#include "shadow_csm.glsl"

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

float computeShadow(vec3 world_position, vec3 normal, vec3 dir_to_light)
{
    ivec2 atlas_size = textureSize(sampler2D(u_ShadowMap, u_Sampler), 0);
    float atlas_texel = 2.0 / float(max(atlas_size.x, atlas_size.y));
    return CsmComputeDirectionalShadow(
        atlas_texel,
        world_position,
        normal,
        dir_to_light,
        u_CameraPosition.xyz,
        normalize(u_CameraDirection.xyz),
        u_CascadeViewProjections,
        u_CascadeSplits,
        u_ShadowParams);
}
const float PI = 3.14159265359;
const float kIblMaxRadiance = 3.0;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 evalIrradianceSH(vec3 n) {
    vec3 result = vec3(0.0);
    result += u_IrradianceSH[0].rgb * 0.282095;
    result += u_IrradianceSH[1].rgb * (0.488603 * n.y);
    result += u_IrradianceSH[2].rgb * (0.488603 * n.z);
    result += u_IrradianceSH[3].rgb * (0.488603 * n.x);
    result += u_IrradianceSH[4].rgb * (1.092548 * n.x * n.z);
    result += u_IrradianceSH[5].rgb * (1.092548 * n.y * n.z);
    result += u_IrradianceSH[6].rgb * (0.315392 * (3.0 * n.z * n.z - 1.0));
    result += u_IrradianceSH[7].rgb * (1.092548 * n.x * n.y);
    result += u_IrradianceSH[8].rgb * (1.092548 * (n.x * n.x - n.y * n.y));
    return max(result, vec3(0.0));
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, 1e-5);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / max(NdotV * (1.0 - k) + k, 1e-5);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

vec3 evaluateDirectPBR(vec3 albedo, vec3 N, vec3 V, vec3 L, vec3 radiance, float metallic, float roughness) {
    vec3 H = normalize(V + L);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);

    vec3 numerator = NDF * G * F;
    float denom = max(4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0), 1e-5);
    vec3 specular = numerator / denom;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

vec3 applyDirectionalLight(vec3 albedo, vec3 normal, vec3 position, float metallic, float roughness)
{
    vec3 n = normalize(normal);
    vec3 v = normalize(u_CameraPosition.xyz - position);
    vec3 light_dir = normalize(-u_DirectionalDirectionFlags.xyz);
    vec3 light_color = u_DirectionalColorIntensity.rgb * u_DirectionalColorIntensity.a;
    float shadow = computeShadow(position, n, light_dir);
    return evaluateDirectPBR(albedo, n, v, light_dir, light_color * shadow, metallic, roughness);
}

vec3 applyPointLight(vec3 albedo, vec3 normal, vec3 position, float metallic, float roughness, int light_index)
{
    vec3 light_color = u_PointLightColors[light_index].rgb * u_PointLightColors[light_index].a * 3.0;
    vec3 light_pos = u_PointLightPositions[light_index].xyz;
    float light_range = max(u_PointLightPositions[light_index].w, 0.001);

    vec3 light_vector = light_pos - position;
    float distance_to_light = length(light_vector);
    if (distance_to_light >= light_range) {
        return vec3(0.0);
    }

    vec3 l = normalize(light_vector);
    vec3 n = normalize(normal);
    vec3 v = normalize(u_CameraPosition.xyz - position);

    float falloff = 1.0 - clamp(distance_to_light / light_range, 0.0, 1.0);
    falloff *= falloff;
    return evaluateDirectPBR(albedo, n, v, l, light_color * falloff, metallic, roughness);
}

vec3 evaluateIBL(vec3 albedo, vec3 N, vec3 V, float metallic, float roughness, float ao) {
    vec3 R = reflect(-V, N);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float NdotV = max(dot(N, V), 0.0);
    vec3 F = fresnelSchlick(NdotV, F0);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 irradiance = evalIrradianceSH(N);
    irradiance = min(irradiance, vec3(kIblMaxRadiance));
    vec3 diffuse = irradiance * albedo;

    float max_lod = max(u_IblParams.z, 1.0);
    vec3 prefiltered = textureLod(samplerCube(u_SkyboxTexture, u_Sampler), R, roughness * max_lod).rgb;
    prefiltered = min(prefiltered, vec3(kIblMaxRadiance));
    vec2 env_brdf = textureLod(sampler2D(u_BrdfLut, u_Sampler), vec2(NdotV, roughness), 0.0).rg;
    vec3 specular = prefiltered * (F * env_brdf.x + env_brdf.y);

    float diffuse_strength = u_IblParams.x;
    float specular_strength = u_IblParams.x * u_IblParams.y * pow(1.0 - roughness, 2.0);
    vec3 ibl_diffuse = kD * diffuse;
    vec3 ibl_specular = specular;
    return (ibl_diffuse * diffuse_strength + ibl_specular * specular_strength) * ao;
}

void main()
{
    vec4 albedo_sample = texture(sampler2D(u_Textures[v_TexIndex], u_TextureSampler), v_UV);
    vec3 albedo = albedo_sample.rgb * v_ColorTint.rgb;
    float metallic = clamp(u_MaterialData.x, 0.0, 1.0);
    float roughness = clamp(u_MaterialData.y, 0.04, 1.0);
    float ao = clamp(u_MaterialData.z, 0.0, 1.0);
    if (u_DrawData.z != 0) {
        vec4 mr_ao = texture(sampler2D(u_Textures[uint(u_DrawData.y)], u_TextureSampler), v_UV);
        metallic = clamp(metallic * mr_ao.b, 0.0, 1.0);
        roughness = clamp(roughness * mr_ao.g, 0.04, 1.0);
        ao = clamp(ao * mr_ao.r, 0.0, 1.0);
    }

    vec3 n = normalize(v_Normal);
    if (u_DrawData.w >= 0) {
        vec3 tangent_space = texture(sampler2D(u_Textures[uint(u_DrawData.w)], u_TextureSampler), v_UV).xyz;
        bool fallback_white = tangent_space.r >= 0.999 && tangent_space.g >= 0.999 && tangent_space.b >= 0.999;
        if (!fallback_white) {
            n = perturbNormal(v_WorldPosition, n, v_UV, tangent_space * 2.0 - 1.0);
        }
    }
    vec3 emissive = u_EmissiveData.rgb;
    if (u_EmissiveData.w > 0.0) {
        vec3 emissive_tex = texture(sampler2D(u_Textures[uint(u_EmissiveData.w) - 1u], u_TextureSampler), v_UV).rgb;
        emissive *= emissive_tex;
    }

    vec3 v = normalize(u_CameraPosition.xyz - v_WorldPosition);
    vec3 color = evaluateIBL(albedo, n, v, metallic, roughness, ao);

    if (u_DirectionalDirectionFlags.w < 0.5) {
        color += applyDirectionalLight(albedo, n, v_WorldPosition, metallic, roughness);
    }

    int point_light_count = int(u_LightCountFlags.x);
    for (int i = 0; i < point_light_count; i++) {
        color += applyPointLight(albedo, n, v_WorldPosition, metallic, roughness, i);
    }

    o_Color = vec4(color + max(emissive, vec3(0.0)), albedo_sample.a * v_ColorTint.a);
}
