#version 450

#include "shader_parameter_sets.glsl"

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_CONSTANTS) uniform TaaConstants {
    mat4 u_PrevViewProjection;
    mat4 u_CurrentViewProjection;
    vec4 u_Params;     // xy: current jitter uv, z: reset flag, w: base blend weight
    vec4 u_PrevParams; // xy: previous frame jitter uv
    vec4 u_Tuning;     // x: variance gamma, y: disocclusion relative depth threshold, z: speed falloff (texels), w: sharpen
};

layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT0) uniform texture2D u_CurrentColor;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT1) uniform texture2D u_HistoryColor;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT2) uniform texture2D u_Position;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT3) uniform texture2D u_MotionVector;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT4) uniform texture2D u_PrevDepth;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_SAMPLER) uniform sampler u_Sampler;

vec2 NdcToUv(vec2 ndc)
{
    return vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
}

vec3 RGBToYCoCg(vec3 color)
{
    return vec3(
        0.25 * color.r + 0.5 * color.g + 0.25 * color.b,
        0.5 * color.r - 0.5 * color.b,
        -0.25 * color.r + 0.5 * color.g - 0.25 * color.b);
}

vec3 YCoCgToRGB(vec3 ycocg)
{
    float y = ycocg.x;
    float co = ycocg.y;
    float cg = ycocg.z;
    return vec3(y + co - cg, y + cg, y - co - cg);
}

void main()
{
    vec2 texel_size = 1.0 / vec2(textureSize(sampler2D(u_CurrentColor, u_Sampler), 0));
    vec2 resolution = vec2(textureSize(sampler2D(u_CurrentColor, u_Sampler), 0));
    vec2 scene_uv = v_UV - u_Params.xy;

    vec3 current = texture(sampler2D(u_CurrentColor, u_Sampler), v_UV).rgb;
    vec4 world = texture(sampler2D(u_Position, u_Sampler), v_UV);
    vec2 motion = texture(sampler2D(u_MotionVector, u_Sampler), v_UV).xy;
    bool has_world = dot(world.xyz, world.xyz) > 0.0;

    vec2 history_uv = scene_uv + u_PrevParams.xy;
    bool has_history = true;
    if (has_world) {
        vec4 prev_clip = u_PrevViewProjection * vec4(world.xyz, 1.0);
        if (prev_clip.w > 0.0001) {
            vec2 prev_uv = NdcToUv(prev_clip.xy / prev_clip.w) + u_PrevParams.xy;
            if (prev_uv.x >= 0.0 && prev_uv.x <= 1.0 && prev_uv.y >= 0.0 && prev_uv.y <= 1.0) {
                history_uv = prev_uv;
            } else {
                has_history = false;
            }
        } else {
            has_history = false;
        }
    }
    if (has_world && (abs(motion.x) > 0.0 || abs(motion.y) > 0.0)) {
        vec2 motion_uv = v_UV - motion;
        if (motion_uv.x >= 0.0 && motion_uv.x <= 1.0 && motion_uv.y >= 0.0 && motion_uv.y <= 1.0) {
            history_uv = motion_uv;
        } else {
            has_history = false;
        }
    }
    if (!has_history) {
        o_Color = vec4(current, 1.0);
        return;
    }

    float disocclusion = 0.0;
    if (has_world) {
        vec4 expected_prev_clip = u_PrevViewProjection * vec4(world.xyz, 1.0);
        if (expected_prev_clip.w > 0.0001) {
            float expected_depth = expected_prev_clip.z / expected_prev_clip.w;
            vec2 depth_texel = texel_size * 2.0;
            float stored_depth = texture(sampler2D(u_PrevDepth, u_Sampler), history_uv).x;
            stored_depth = min(stored_depth, texture(sampler2D(u_PrevDepth, u_Sampler), history_uv + vec2(-depth_texel.x, 0.0)).x);
            stored_depth = min(stored_depth, texture(sampler2D(u_PrevDepth, u_Sampler), history_uv + vec2(depth_texel.x, 0.0)).x);
            stored_depth = min(stored_depth, texture(sampler2D(u_PrevDepth, u_Sampler), history_uv + vec2(0.0, -depth_texel.y)).x);
            stored_depth = min(stored_depth, texture(sampler2D(u_PrevDepth, u_Sampler), history_uv + vec2(0.0, depth_texel.y)).x);
            float depth_threshold = max(0.005, u_Tuning.y * expected_depth);
            disocclusion = abs(stored_depth - expected_depth) > depth_threshold ? 1.0 : 0.0;
        } else {
            disocclusion = 1.0;
        }
    }

    vec3 ycc_min = RGBToYCoCg(current);
    vec3 ycc_max = ycc_min;
    vec3 ycc_sum = vec3(0.0);
    vec3 ycc_square_sum = vec3(0.0);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec3 sample_color = texture(sampler2D(u_CurrentColor, u_Sampler), scene_uv + vec2(x, y) * texel_size).rgb;
            vec3 sample_ycc = RGBToYCoCg(sample_color);
            ycc_min = min(ycc_min, sample_ycc);
            ycc_max = max(ycc_max, sample_ycc);
            ycc_sum += sample_ycc;
            ycc_square_sum += sample_ycc * sample_ycc;
        }
    }
    vec3 ycc_mean = ycc_sum / 9.0;
    vec3 ycc_variance = max(ycc_square_sum / 9.0 - ycc_mean * ycc_mean, vec3(0.0));
    vec3 ycc_sigma = sqrt(ycc_variance);
    vec3 box_min = max(ycc_min, ycc_mean - u_Tuning.x * ycc_sigma);
    vec3 box_max = min(ycc_max, ycc_mean + u_Tuning.x * ycc_sigma);
    box_min = min(box_min, box_max);

    vec3 history_raw = textureLod(sampler2D(u_HistoryColor, u_Sampler), history_uv, 0.0).rgb;
    vec3 history_ycc = RGBToYCoCg(history_raw);
    vec3 clamped_ycc = clamp(history_ycc, box_min, box_max);
    vec3 clamped_history = YCoCgToRGB(clamped_ycc);

    float alpha = u_Params.w;
    if (u_Params.z < 0.5) {
        float deviation = clamp(length(history_ycc - clamped_ycc) * 6.0, 0.0, 1.0);
        float speed = clamp(length(motion * resolution) / max(u_Tuning.z, 1.0), 0.0, 1.0);
        alpha -= alpha * deviation * 0.85;
        alpha = min(alpha, mix(alpha, 0.35, speed));
        alpha = mix(alpha, 0.0, disocclusion);
        alpha = clamp(alpha, 0.0, u_Params.w);
    }

    vec3 result = mix(current, clamped_history, alpha);

    vec3 neighbor_blur = (
        texture(sampler2D(u_CurrentColor, u_Sampler), scene_uv + vec2(0.0, -1.0) * texel_size).rgb +
        texture(sampler2D(u_CurrentColor, u_Sampler), scene_uv + vec2(0.0, 1.0) * texel_size).rgb +
        texture(sampler2D(u_CurrentColor, u_Sampler), scene_uv + vec2(-1.0, 0.0) * texel_size).rgb +
        texture(sampler2D(u_CurrentColor, u_Sampler), scene_uv + vec2(1.0, 0.0) * texel_size).rgb) * 0.25;
    result += (result - neighbor_blur) * u_Tuning.w;

    o_Color = vec4(result, 1.0);
}
