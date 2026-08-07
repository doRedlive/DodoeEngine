#version 450

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

layout(set = 2, binding = 0) uniform texture2D u_InputTexture;
layout(set = 1, binding = 0) uniform sampler u_Sampler;

vec3 SampleColor(vec2 uv)
{
    return texture(sampler2D(u_InputTexture, u_Sampler), uv).rgb;
}

void main()
{
    vec2 texel_size = 1.0 / vec2(textureSize(sampler2D(u_InputTexture, u_Sampler), 0));
    vec3 rgb_nw = SampleColor(v_UV + texel_size * vec2(-1.0, -1.0));
    vec3 rgb_ne = SampleColor(v_UV + texel_size * vec2( 1.0, -1.0));
    vec3 rgb_sw = SampleColor(v_UV + texel_size * vec2(-1.0,  1.0));
    vec3 rgb_se = SampleColor(v_UV + texel_size * vec2( 1.0,  1.0));
    vec3 rgb_m  = SampleColor(v_UV);

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float luma_nw = dot(rgb_nw, luma);
    float luma_ne = dot(rgb_ne, luma);
    float luma_sw = dot(rgb_sw, luma);
    float luma_se = dot(rgb_se, luma);
    float luma_m = dot(rgb_m, luma);

    float luma_min = min(luma_m, min(min(luma_nw, luma_ne), min(luma_sw, luma_se)));
    float luma_max = max(luma_m, max(max(luma_nw, luma_ne), max(luma_sw, luma_se)));

    vec2 dir = vec2(-((luma_nw + luma_ne) - (luma_sw + luma_se)), ((luma_nw + luma_sw) - (luma_ne + luma_se)));
    float dir_reduce = max((luma_nw + luma_ne + luma_sw + luma_se) * 0.25 * 0.5, 1.0 / 128.0);
    float rcp_dir_min = 1.0 / (min(abs(dir.x), abs(dir.y)) + dir_reduce);
    dir = clamp(dir * rcp_dir_min * texel_size, vec2(-8.0), vec2(8.0));

    vec3 rgb_a = 0.5 * (
        SampleColor(v_UV + dir * (1.0 / 3.0 - 0.5)) +
        SampleColor(v_UV + dir * (2.0 / 3.0 - 0.5)));
    vec3 rgb_b = rgb_a * 0.5 + 0.25 * (
        SampleColor(v_UV + dir * -0.5) +
        SampleColor(v_UV + dir * 0.5));

    float luma_b = dot(rgb_b, luma);
    if (luma_b < luma_min || luma_b > luma_max) {
        o_Color = vec4(rgb_a, 1.0);
    } else {
        o_Color = vec4(rgb_b, 1.0);
    }
}