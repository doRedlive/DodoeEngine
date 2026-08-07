#version 450 core

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

layout(set = 0, binding = 0) uniform DeferredLightPassUBO {
    vec4 u_LightColorIntensity;
    vec4 u_LightPositionRadius;
    vec4 u_LightDirectionType;
    mat4 u_LightViewProjection;
    vec4 u_ShadowParams;
    vec4 u_CameraPosition;
};

layout(set = 2, binding = 0) uniform texture2D u_Albedo;
layout(set = 2, binding = 1) uniform texture2D u_Normal;
layout(set = 2, binding = 2) uniform texture2D u_Position;
layout(set = 2, binding = 3) uniform texture2D u_ShadowMap;
layout(set = 2, binding = 4) uniform texture2D u_Material;
layout(set = 2, binding = 5) uniform textureCube u_SkyboxTexture;
layout(set = 1, binding = 0) uniform sampler u_Sampler;

const vec2 poissonDisk[16] = vec2[](
    vec2( -0.94201624, -0.39906216 ),
    vec2( 0.94558609, -0.76890725 ),
    vec2( -0.094184101, -0.92938870 ),
    vec2( 0.34495938, 0.29387760 ),
    vec2( -0.91588581, 0.45771432 ),
    vec2( -0.81544232, -0.87912464 ),
    vec2( -0.38277543, 0.27676845 ),
    vec2( 0.97484398, 0.75648379 ),
    vec2( 0.44323325, -0.97511554 ),
    vec2( 0.53742981, -0.47373420 ),
    vec2( -0.26496911, -0.41893023 ),
    vec2( 0.79197514, 0.19090188 ),
    vec2( -0.24188840, 0.99706507 ),
    vec2( -0.81409955, 0.91437590 ),
    vec2( 0.19984126, 0.78641367 ),
    vec2( 0.14383161, -0.14100790 )
);

float rand_2to1(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float findBlocker(vec2 uv, float zReceiver, float searchRadius) {
    int blockers = 0;
    float blockDepthSum = 0.0;
    
    float angle = rand_2to1(uv) * 6.2831853;
    float s = sin(angle), c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);

    for (int i = 0; i < 16; i++) {
        vec2 sampleUV = uv + rot * poissonDisk[i] * searchRadius;
        
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) {
            continue;
        }
        
        float mapDepth = texture(sampler2D(u_ShadowMap, u_Sampler), sampleUV).r;
        if (mapDepth < zReceiver) {
            blockDepthSum += mapDepth;
            blockers++;
        }
    }
    
    if (blockers == 0) return -1.0;
    return blockDepthSum / float(blockers);
}

float PCF(vec2 uv, float zReceiver, float filterRadius) {
    float sum = 0.0;
    
    float angle = rand_2to1(uv) * 6.2831853;
    float s = sin(angle), c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);

    for (int i = 0; i < 16; i++) {
        vec2 sampleUV = uv + rot * poissonDisk[i] * filterRadius;
        
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) {
            sum += 1.0;
            continue;
        }
        
        float mapDepth = texture(sampler2D(u_ShadowMap, u_Sampler), sampleUV).r;
        sum += (zReceiver <= mapDepth) ? 1.0 : u_ShadowParams.y;
    }
    return sum / 16.0;
}

float computeShadow(vec3 world_position, vec3 normal, vec3 light_dir)
{
    vec2 texSize = vec2(textureSize(sampler2D(u_ShadowMap, u_Sampler), 0));
    float texelSize = 1.0 / max(texSize.x, texSize.y);

    // Keep a minimum normal offset in light clip depth units to suppress self-shadowing acne.
    float normalOffset = max(u_ShadowParams.z, texelSize * 2.0);
    vec3 shadow_position = world_position + normal * normalOffset;
    vec4 light_clip = u_LightViewProjection * vec4(shadow_position, 1.0);
    if (light_clip.w <= 0.0) {
        return 1.0;
    }

    vec3 light_ndc = light_clip.xyz / light_clip.w;
    vec2 shadow_uv = light_ndc.xy * 0.5 + 0.5;
    float shadow_depth = light_ndc.z;

    if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 || shadow_uv.y < 0.0 || shadow_uv.y > 1.0) {
        return 1.0;
    }

    if (light_ndc.z < 0.0 || light_ndc.z > 1.0) {
        return 1.0;
    }

    float ndotl = max(dot(normal, -light_dir), 0.0);
    float bias = max(u_ShadowParams.x * (1.0 - ndotl), texelSize * 1.5);
    float zReceiver = shadow_depth - bias;
    
    float lightSize = u_ShadowParams.w > 0.0 ? u_ShadowParams.w : 2.0;
    float searchRadius = lightSize * texelSize * 2.0;

    // STEP 1: Blocker Search
    float avgBlockerDepth = findBlocker(shadow_uv, zReceiver, searchRadius);
    
    if (avgBlockerDepth < 0.0) {
        return 1.0;
    }

    // STEP 2: Penumbra Estimation
    float penumbraRatio = max(zReceiver - avgBlockerDepth, 0.0) * 15.0;
    float filterRadius = max(penumbraRatio * lightSize * texelSize, texelSize);

    // STEP 3: PCF Filtering
    return PCF(shadow_uv, zReceiver, filterRadius);
}

const float PI = 3.14159265359;
const float kIblDiffuseStrength = 0.2;
const float kIblSpecularStrength = 0.35;
const float kIblMaxRadiance = 3.0;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
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
    vec3 light_dir = normalize(-u_LightDirectionType.xyz);
    vec3 light_color = u_LightColorIntensity.rgb * u_LightColorIntensity.a;
    float shadow = computeShadow(position, n, -light_dir);
    return evaluateDirectPBR(albedo, n, v, light_dir, light_color * shadow, metallic, roughness);
}

vec3 applyPointLight(vec3 albedo, vec3 normal, vec3 position, float metallic, float roughness)
{
    vec3 light_color = u_LightColorIntensity.rgb * u_LightColorIntensity.a * 3.0;
    vec3 light_pos = u_LightPositionRadius.xyz;
    float light_radius = max(u_LightPositionRadius.w, 0.001);
    float light_range = max(u_LightDirectionType.w, light_radius);

    vec3 light_vector = light_pos - position;
    float distance_to_light = length(light_vector);
    if (distance_to_light >= light_range) {
        return vec3(0.0);
    }

    vec3 l = normalize(light_vector);
    vec3 n = normalize(normal);
    vec3 v = normalize(u_CameraPosition.xyz - position);

    float falloff = 1.0 - clamp((distance_to_light - light_radius) / max(light_range - light_radius, 0.001), 0.0, 1.0);
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

    // Use high LOD as a cheap irradiance approximation (avoid strong sky tinting).
    float max_lod = 5.0;
    vec3 irradiance = textureLod(samplerCube(u_SkyboxTexture, u_Sampler), N, max_lod).rgb;
    irradiance = min(irradiance, vec3(kIblMaxRadiance));
    vec3 diffuse = irradiance * albedo;

    // First-pass IBL: roughness LOD prefilter approximation.
    vec3 prefiltered = textureLod(samplerCube(u_SkyboxTexture, u_Sampler), R, roughness * max_lod).rgb;
    prefiltered = min(prefiltered, vec3(kIblMaxRadiance));
    // UE4-style BRDF approximation; avoids overly bright constant specular term.
    vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    vec2 env_brdf = vec2(-1.04, 1.04) * a004 + r.zw;
    vec3 specular = prefiltered * (F * env_brdf.x + env_brdf.y);

    vec3 ibl_diffuse = kD * diffuse / PI;
    vec3 ibl_specular = specular;
    return (ibl_diffuse * kIblDiffuseStrength + ibl_specular * kIblSpecularStrength) * ao;
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
    vec3 color = evaluateIBL(albedo, n, v, metallic, roughness, ao);

    if (u_LightDirectionType.w < 0.5) {
        color += applyDirectionalLight(albedo, normal, position, metallic, roughness);
        o_Color = vec4(color, 1.0);
        return;
    }

    color += applyPointLight(albedo, normal, position, metallic, roughness);
    o_Color = vec4(color, 1.0);
}
