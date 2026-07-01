cbuffer DeferredLightPassUBO : register(b0)
{
    float4   u_LightColorIntensity;
    float4   u_LightPositionRadius;
    float4   u_LightDirectionType;
    float4x4 u_LightViewProjection;
    float4   u_ShadowParams;
    float4   u_CameraPosition;
};

Texture2D   u_Albedo      : register(t0);
Texture2D   u_Normal      : register(t1);
Texture2D   u_Position    : register(t2);
Texture2D   u_ShadowMap   : register(t3);
Texture2D   u_Material    : register(t4);
TextureCube u_SkyboxTexture : register(t5);
SamplerState u_Sampler    : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 v_UV     : TEXCOORD0;
};

static const float2 poissonDisk[16] =
{
    float2(-0.94201624, -0.39906216), float2( 0.94558609, -0.76890725),
    float2(-0.094184101, -0.92938870), float2( 0.34495938,  0.29387760),
    float2(-0.91588581,  0.45771432), float2(-0.81544232, -0.87912464),
    float2(-0.38277543,  0.27676845), float2( 0.97484398,  0.75648379),
    float2( 0.44323325, -0.97511554), float2( 0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023), float2( 0.79197514,  0.19090188),
    float2(-0.24188840,  0.99706507), float2(-0.81409955,  0.91437590),
    float2( 0.19984126,  0.78641367), float2( 0.14383161, -0.14100790)
};

float rand_2to1(float2 co) { return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453); }

float findBlocker(float2 uv, float zReceiver, float searchRadius)
{
    int blockers = 0;
    float blockDepthSum = 0.0;
    float angle = rand_2to1(uv) * 6.2831853;
    float s = sin(angle), c = cos(angle);
    float2x2 rot = float2x2(c, -s, s, c);
    for (int i = 0; i < 16; i++)
    {
        float2 sampleUV = uv + mul(rot, poissonDisk[i]) * searchRadius;
        if (any(sampleUV < 0.0) || any(sampleUV > 1.0)) continue;
        float mapDepth = u_ShadowMap.Sample(u_Sampler, sampleUV).r;
        if (mapDepth < zReceiver) { blockDepthSum += mapDepth; blockers++; }
    }
    if (blockers == 0) return -1.0;
    return blockDepthSum / float(blockers);
}

float PCF(float2 uv, float zReceiver, float filterRadius)
{
    float sum = 0.0;
    float angle = rand_2to1(uv) * 6.2831853;
    float s = sin(angle), c = cos(angle);
    float2x2 rot = float2x2(c, -s, s, c);
    for (int i = 0; i < 16; i++)
    {
        float2 sampleUV = uv + mul(rot, poissonDisk[i]) * filterRadius;
        if (any(sampleUV < 0.0) || any(sampleUV > 1.0)) { sum += 1.0; continue; }
        float mapDepth = u_ShadowMap.Sample(u_Sampler, sampleUV).r;
        sum += (zReceiver <= mapDepth) ? 1.0 : u_ShadowParams.y;
    }
    return sum / 16.0;
}

float computeShadow(float3 world_position, float3 normal, float3 light_dir)
{
    uint2 texSize; u_ShadowMap.GetDimensions(texSize.x, texSize.y);
    float texelSize = 1.0 / max(float(texSize.x), float(texSize.y));
    float normalOffset = max(u_ShadowParams.z, texelSize * 2.0);
    float3 shadow_position = world_position + normal * normalOffset;
    float4 light_clip = mul(u_LightViewProjection, float4(shadow_position, 1.0));
    if (light_clip.w <= 0.0) return 1.0;
    float3 light_ndc = light_clip.xyz / light_clip.w;
    float2 shadow_uv = light_ndc.xy * 0.5 + 0.5;
    float shadow_depth = light_ndc.z;
    if (any(shadow_uv < 0.0) || any(shadow_uv > 1.0)) return 1.0;
    if (light_ndc.z < 0.0 || light_ndc.z > 1.0) return 1.0;
    float ndotl = max(dot(normal, -light_dir), 0.0);
    float bias = max(u_ShadowParams.x * (1.0 - ndotl), texelSize * 1.5);
    float zReceiver = shadow_depth - bias;
    float lightSize = u_ShadowParams.w > 0.0 ? u_ShadowParams.w : 2.0;
    float searchRadius = lightSize * texelSize * 2.0;
    float avgBlockerDepth = findBlocker(shadow_uv, zReceiver, searchRadius);
    if (avgBlockerDepth < 0.0) return 1.0;
    float penumbraRatio = max(zReceiver - avgBlockerDepth, 0.0) * 15.0;
    float filterRadius = max(penumbraRatio * lightSize * texelSize, texelSize);
    return PCF(shadow_uv, zReceiver, filterRadius);
}

static const float PI = 3.14159265359;
static const float kIblDiffuseStrength = 0.2;
static const float kIblSpecularStrength = 0.35;
static const float kIblMaxRadiance = 3.0;

float3 fresnelSchlick(float cosTheta, float3 F0) { return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); }

float distributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness, a2 = a * a;
    float NdotH = max(dot(N, H), 0.0), NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, 1e-5);
}

float geometrySchlickGGX(float NdotV, float roughness) { float r = roughness + 1.0, k = (r * r) / 8.0; return NdotV / max(NdotV * (1.0 - k) + k, 1e-5); }
float geometrySmith(float3 N, float3 V, float3 L, float roughness) { return geometrySchlickGGX(max(dot(N, V), 0.0), roughness) * geometrySchlickGGX(max(dot(N, L), 0.0), roughness); }

float3 evaluateDirectPBR(float3 albedo, float3 N, float3 V, float3 L, float3 radiance, float metallic, float roughness)
{
    float3 H = normalize(V + L);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    float3 numerator = NDF * G * F;
    float denom = max(4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0), 1e-5);
    float3 specular = numerator / denom;
    float3 kS = F;
    float3 kD = (float3(1.0, 1.0, 1.0) - kS) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

float3 applyDirectionalLight(float3 albedo, float3 normal, float3 position, float metallic, float roughness)
{
    float3 n = normalize(normal);
    float3 v = normalize(u_CameraPosition.xyz - position);
    float3 light_dir = normalize(-u_LightDirectionType.xyz);
    float3 light_color = u_LightColorIntensity.rgb * u_LightColorIntensity.a;
    float shadow = computeShadow(position, n, -light_dir);
    return evaluateDirectPBR(albedo, n, v, light_dir, light_color * shadow, metallic, roughness);
}

float3 applyPointLight(float3 albedo, float3 normal, float3 position, float metallic, float roughness)
{
    float3 light_color = u_LightColorIntensity.rgb * u_LightColorIntensity.a * 3.0;
    float3 light_pos = u_LightPositionRadius.xyz;
    float light_radius = max(u_LightPositionRadius.w, 0.001);
    float light_range = max(u_LightDirectionType.w, light_radius);
    float3 light_vector = light_pos - position;
    float distance_to_light = length(light_vector);
    if (distance_to_light >= light_range) return float3(0.0, 0.0, 0.0);
    float3 l = normalize(light_vector);
    float3 n = normalize(normal);
    float3 v = normalize(u_CameraPosition.xyz - position);
    float falloff = 1.0 - clamp((distance_to_light - light_radius) / max(light_range - light_radius, 0.001), 0.0, 1.0);
    falloff *= falloff;
    return evaluateDirectPBR(albedo, n, v, l, light_color * falloff, metallic, roughness);
}

float3 evaluateIBL(float3 albedo, float3 N, float3 V, float metallic, float roughness, float ao)
{
    float3 R = reflect(-V, N);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float NdotV = max(dot(N, V), 0.0);
    float3 F = fresnelSchlick(NdotV, F0);
    float3 kS = F, kD = (float3(1.0, 1.0, 1.0) - kS) * (1.0 - metallic);
    float max_lod = 5.0;
    float3 irradiance = min(u_SkyboxTexture.SampleLevel(u_Sampler, N, max_lod).rgb, float3(kIblMaxRadiance, kIblMaxRadiance, kIblMaxRadiance));
    float3 diffuse = irradiance * albedo;
    float3 prefiltered = min(u_SkyboxTexture.SampleLevel(u_Sampler, R, roughness * max_lod).rgb, float3(kIblMaxRadiance, kIblMaxRadiance, kIblMaxRadiance));
    float4 c0 = float4(-1.0, -0.0275, -0.572, 0.022), c1 = float4(1.0, 0.0425, 1.04, -0.04);
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    float2 env_brdf = float2(-1.04, 1.04) * a004 + r.zw;
    float3 specular = prefiltered * (F * env_brdf.x + env_brdf.y);
    return (kD * diffuse / PI * kIblDiffuseStrength + specular * kIblSpecularStrength) * ao;
}

float4 main(PSInput input) : SV_Target
{
    float3 albedo   = u_Albedo.Sample(u_Sampler, input.v_UV).rgb;
    float3 normal   = u_Normal.Sample(u_Sampler, input.v_UV).xyz;
    float3 position = u_Position.Sample(u_Sampler, input.v_UV).xyz;
    float3 material = u_Material.Sample(u_Sampler, input.v_UV).rgb;
    float metallic  = clamp(material.r, 0.0, 1.0);
    float roughness = clamp(material.g, 0.04, 1.0);
    float ao        = clamp(material.b, 0.0, 1.0);

    if (length(normal) < 0.1) return float4(0.0, 0.0, 0.0, 0.0);

    float3 n = normalize(normal);
    float3 v = normalize(u_CameraPosition.xyz - position);

    float3 color = evaluateIBL(albedo, n, v, metallic, roughness, ao);

    if (u_LightDirectionType.w < 0.5)
    {
        color += applyDirectionalLight(albedo, normal, position, metallic, roughness);
        return float4(color, 1.0);
    }
    color += applyPointLight(albedo, normal, position, metallic, roughness);
    return float4(color, 1.0);
}
