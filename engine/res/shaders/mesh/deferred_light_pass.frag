#version 450 core

#include "common/shader_parameter_sets.glsl"

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

const uint kLightTypeDirectional = 0u;
const uint kLightTypePoint = 1u;
const uint kLightTypeSpot = 2u;

struct DeferredLightData {
    vec4 color_intensity;   // rgb color, a intensity
    vec4 position_radius;   // xyz world position, w radius
    vec4 direction_range;   // xyz direction, w range
    vec4 params;            // x inner_angle(rad), y outer_angle(rad), z type, w casts_shadow
};

layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_CONSTANTS) uniform DeferredLightPassUBO {
    vec4 u_CameraDirection;
    vec4 u_CameraPosition;          // w = has_sky
    vec4 u_ShadowParams;
    mat4 u_CascadeViewProjections[4];
    vec4 u_CascadeSplits;
    vec4 u_IrradianceSH[9];
    vec4 u_IblParams;
    vec4 u_EmissiveParams;
    vec4 u_LightInfo;               // x = light count
};

layout(std430, set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT7) readonly buffer DeferredLights {
    DeferredLightData u_Lights[];
};

layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT0) uniform texture2D u_Albedo;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT1) uniform texture2D u_Normal;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT2) uniform texture2D u_Position;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT3) uniform texture2D u_ShadowMap;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT4) uniform texture2D u_Material;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT5) uniform textureCube u_SkyboxTexture;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT6) uniform texture2D u_BrdfLut;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_SAMPLER) uniform sampler u_Sampler;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT8) uniform texture2D u_Emissive;

#include "common/shadow_csm.glsl"

const float PI = 3.14159265359;
const float kIblMaxRadiance = 3.0;

float computeShadow(vec3 world_position, vec3 normal, vec3 dir_to_light) {
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

vec3 applyPunctualLight(DeferredLightData light, vec3 albedo, vec3 n, vec3 v, vec3 position,
                        float metallic, float roughness) {
    vec3 light_color = light.color_intensity.rgb * light.color_intensity.a * 3.0;
    vec3 light_pos = light.position_radius.xyz;
    float light_radius = max(light.position_radius.w, 0.001);
    float light_range = max(light.direction_range.w, light_radius);

    vec3 light_vector = light_pos - position;
    float distance_to_light = length(light_vector);
    if (distance_to_light >= light_range) {
        return vec3(0.0);
    }

    vec3 l = normalize(light_vector);
    float falloff = 1.0 - clamp((distance_to_light - light_radius) / max(light_range - light_radius, 0.001), 0.0, 1.0);
    falloff *= falloff;

    float spot_factor = 1.0;
    if (uint(light.params.z + 0.5) == kLightTypeSpot) {
        float theta = dot(l, normalize(light.direction_range.xyz));
        float inner = cos(light.params.x);
        float outer = cos(light.params.y);
        spot_factor = clamp((theta - outer) / max(inner - outer, 1e-4), 0.0, 1.0);
    }

    return evaluateDirectPBR(albedo, n, v, l, light_color * falloff * spot_factor, metallic, roughness);
}

vec3 evaluateIBL(vec3 albedo, vec3 N, vec3 V, float metallic, float roughness, float ao,
                 float diffuse_strength, float specular_strength) {
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

    vec3 ibl_diffuse = kD * diffuse;
    vec3 ibl_specular = specular;
    return (ibl_diffuse * diffuse_strength + ibl_specular * specular_strength) * ao;
}

void main()
{
    vec3 albedo = texture(sampler2D(u_Albedo, u_Sampler), v_UV).rgb;
    vec3 normal = texture(sampler2D(u_Normal, u_Sampler), v_UV).xyz;
    vec3 position = texture(sampler2D(u_Position, u_Sampler), v_UV).xyz;
    vec3 material = texture(sampler2D(u_Material, u_Sampler), v_UV).rgb;
    float metallic = clamp(material.r, 0.0, 1.0);
    float roughness = clamp(material.g, 0.04, 1.0);
    float ao = clamp(material.b, 0.0, 1.0);

    if (length(normal) < 0.1) {
        o_Color = vec4(0.0);
        return;
    }
    vec3 n = normalize(normal);
    vec3 v = normalize(u_CameraPosition.xyz - position);

    float diffuse_strength = u_IblParams.x;
    float specular_strength = u_IblParams.x * u_IblParams.y * pow(1.0 - roughness, 2.0);
    vec3 color = evaluateIBL(albedo, n, v, metallic, roughness, ao, diffuse_strength, specular_strength);

    if (u_EmissiveParams.x > 0.5) {
        color += max(texture(sampler2D(u_Emissive, u_Sampler), v_UV).rgb, vec3(0.0));
    }

    const uint light_count = uint(u_LightInfo.x + 0.5);
    for (uint i = 0u; i < light_count; ++i) {
        DeferredLightData light = u_Lights[i];
        const uint light_type = uint(light.params.z + 0.5);
        if (light_type == kLightTypeDirectional) {
            vec3 light_dir = normalize(-light.direction_range.xyz);
            float shadow = light.params.w > 0.5
                ? computeShadow(position, n, light_dir)
                : 1.0;
            color += evaluateDirectPBR(albedo, n, v, light_dir,
                light.color_intensity.rgb * light.color_intensity.a * shadow,
                metallic, roughness);
        } else if (light_type == kLightTypePoint || light_type == kLightTypeSpot) {
            color += applyPunctualLight(light, albedo, n, v, position, metallic, roughness);
        }
    }

    o_Color = vec4(color, 1.0);
}
