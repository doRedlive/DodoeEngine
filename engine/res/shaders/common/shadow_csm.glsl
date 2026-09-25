// do@Redlive

#ifndef DOE_SHADOW_CSM_GLSL
#define DOE_SHADOW_CSM_GLSL

const vec2 kCsmPoissonDisk[16] = vec2[](
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

float CsmRand2To1(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec2 CsmCascadeAtlasOrigin(int cascade) {
    return vec2(float(cascade & 1), float(cascade >> 1)) * 0.5;
}

float CsmFindBlocker(vec2 atlas_origin, vec2 quadrant_uv,
                     float z_receiver, float search_radius) {
    int blockers = 0;
    float block_depth_sum = 0.0;

    float angle = CsmRand2To1(quadrant_uv) * 6.2831853;
    float s = sin(angle), c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);

    for (int i = 0; i < 16; i++) {
        vec2 sample_uv = quadrant_uv + rot * kCsmPoissonDisk[i] * search_radius;
        if (sample_uv.x < 0.0 || sample_uv.x > 1.0 || sample_uv.y < 0.0 || sample_uv.y > 1.0) {
            continue;
        }
        float map_depth = texture(sampler2D(u_ShadowMap, u_Sampler), atlas_origin + sample_uv * 0.5).r;
        if (map_depth < z_receiver) {
            block_depth_sum += map_depth;
            blockers++;
        }
    }

    if (blockers == 0) return -1.0;
    return block_depth_sum / float(blockers);
}

float CsmPcf(vec2 atlas_origin, vec2 quadrant_uv,
             float z_receiver, float filter_radius, float shadow_intensity) {
    float sum = 0.0;

    float angle = CsmRand2To1(quadrant_uv) * 6.2831853;
    float s = sin(angle), c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);

    for (int i = 0; i < 16; i++) {
        vec2 sample_uv = quadrant_uv + rot * kCsmPoissonDisk[i] * filter_radius;
        if (sample_uv.x < 0.0 || sample_uv.x > 1.0 || sample_uv.y < 0.0 || sample_uv.y > 1.0) {
            sum += 1.0;
            continue;
        }
        float map_depth = texture(sampler2D(u_ShadowMap, u_Sampler), atlas_origin + sample_uv * 0.5).r;
        // A zero depth is treated as invalid rather than an occluder. This
        // protects against untouched/invalid atlas tiles appearing as a
        // solid black cascade.
        bool invalid_depth = map_depth <= 1e-5 || map_depth >= 0.99999;
        sum += (invalid_depth || z_receiver <= map_depth) ? 1.0 : shadow_intensity;
    }
    return sum / 16.0;
}

float CsmSampleCascade(float atlas_texel,
                       vec3 world_position,
                       vec3 normal,
                       vec3 dir_to_light,
                       mat4 cascade_vp,
                       int cascade,
                       vec4 shadow_params) {
    float ortho_width = 2.0 / max(abs(cascade_vp[0][0]), 1e-8);
    float uv_per_world = 1.0 / max(ortho_width, 1e-8);
    float world_texel = ortho_width * atlas_texel;
    float ndc_per_world = abs(cascade_vp[2][2]);

    float normal_offset = shadow_params.z > 0.0 ? shadow_params.z : world_texel * 2.0;
    vec4 light_clip = cascade_vp *
        vec4(world_position + normal * normal_offset, 1.0);
    if (light_clip.w <= 0.0) {
        return 1.0;
    }

    vec3 light_ndc = light_clip.xyz / light_clip.w;
    if (light_ndc.z < 0.0 || light_ndc.z > 1.0) {
        return 1.0;
    }
    vec2 quadrant_uv = vec2(light_ndc.x * 0.5 + 0.5, 0.5 - light_ndc.y * 0.5);
    if (quadrant_uv.x < 0.0 || quadrant_uv.x > 1.0 || quadrant_uv.y < 0.0 || quadrant_uv.y > 1.0) {
        return 1.0;
    }

    float ndotl = max(dot(normal, dir_to_light), 0.0);
    float slope_scale = 1.0 + (1.0 - ndotl) * 8.0;
    float bias = world_texel * 1.5 * slope_scale * ndc_per_world;
    float z_receiver = light_ndc.z - bias;

    vec2 atlas_origin = CsmCascadeAtlasOrigin(cascade);
    // Use a bounded PCF radius for the base CSM path. The previous blocker
    // search could classify an entire atlas quadrant as blocked when the
    // receiver and caster depth ranges diverged, producing a solid black
    // square. Keep the filter in texel units and let the bias handle acne.
    float filter_radius = clamp(
        (shadow_params.w > 0.0 ? shadow_params.w : 1.5) * atlas_texel,
        atlas_texel, atlas_texel * 4.0);
    float shadow = CsmPcf(atlas_origin, quadrant_uv, z_receiver,
                          filter_radius, shadow_params.y);
    return clamp(shadow, shadow_params.y, 1.0);
}

float CsmComputeDirectionalShadow(float atlas_texel,
                                  vec3 world_position,
                                  vec3 normal,
                                  vec3 dir_to_light,
                                  vec3 camera_position,
                                  vec3 camera_forward,
                                  mat4 cascade_view_projections[4],
                                  vec4 cascade_splits,
                                  vec4 shadow_params) {
    float view_depth = dot(world_position - camera_position, camera_forward);
    if (view_depth < 0.0 || view_depth >= cascade_splits.w) {
        return 1.0;
    }

    int cascade = 3;
    if (view_depth < cascade_splits.x) {
        cascade = 0;
    } else if (view_depth < cascade_splits.y) {
        cascade = 1;
    } else if (view_depth < cascade_splits.z) {
        cascade = 2;
    }

    float shadow = CsmSampleCascade(atlas_texel, world_position, normal, dir_to_light,
                                    cascade_view_projections[cascade], cascade, shadow_params);
    if (cascade < 3) {
        float split = cascade_splits[cascade];
        float previous_split = cascade == 0 ? 0.0 : cascade_splits[cascade - 1];
        float blend_width = max((split - previous_split) * 0.1, 1.0);
        float blend_start = split - blend_width;
        if (view_depth > blend_start) {
            float next_shadow = CsmSampleCascade(atlas_texel, world_position, normal, dir_to_light,
                                                 cascade_view_projections[cascade + 1],
                                                 cascade + 1, shadow_params);
            float blend = clamp((view_depth - blend_start) / blend_width, 0.0, 1.0);
            shadow = mix(shadow, next_shadow, blend);
        }
    }
    return shadow;
}

#endif
