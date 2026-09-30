#version 450 core

// Infinite editor ground grid, Unity-style: per-pixel ray/plane intersection,
// analytically anti-aliased lines (fwidth), density-driven fade for minor/major
// lines, distance fade toward the horizon. Occlusion is done by manually
// comparing the analytic grid depth against the scene depth SRV (the same
// pattern the skybox pass uses for its non-depth-test variant).

#include "common/shader_parameter_sets.glsl"

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_CONSTANTS) uniform EditorGridConstants {
    mat4 u_InverseViewProjection;
    mat4 u_ViewProjection;
    vec4 u_CameraWorld;   // xyz: camera world position
    vec4 u_Plane;          // xyz: plane normal, w: plane distance
    vec4 u_Params;        // x: minor spacing, y: major spacing, z: fade begin, w: fade end
};

layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_INPUT0) uniform texture2D u_SceneDepth;
layout(set = DOE_SET_PASS, binding = DOE_PASS_BINDING_SAMPLER) uniform sampler u_Sampler;

float gridLine(vec2 coord, float spacing, vec2 derivative) {
    vec2 c = coord / max(spacing, 1e-4);
    vec2 line_width = fwidth(c) + vec2(1e-5);
    vec2 g = abs(fract(c - 0.5) - 0.5) / line_width;
    return 1.0 - clamp(min(g.x, g.y), 0.0, 1.0);
}

float axisLine(float coord, float derivative) {
    float d = abs(coord) / max(derivative * 1.5, 1e-5);
    return 1.0 - clamp(d, 0.0, 1.0);
}

void main()
{
    o_Color = vec4(0.0);

    vec2 ndc = vec2(v_UV.x * 2.0 - 1.0, 1.0 - 2.0 * v_UV.y);

    vec4 near_point = u_InverseViewProjection * vec4(ndc, 0.0, 1.0);
    vec4 far_point = u_InverseViewProjection * vec4(ndc, 1.0, 1.0);
    if (abs(near_point.w) < 1e-6 || abs(far_point.w) < 1e-6) {
        discard;
    }
    vec3 origin = near_point.xyz / near_point.w;
    vec3 ray = far_point.xyz / far_point.w - origin;
    if (!(dot(ray, ray) > 1e-12)) {
        discard;
    }

    vec3 plane_n = normalize(u_Plane.xyz + vec3(1e-6, 0.0, 0.0));
    float denom = dot(plane_n, ray);
    if (abs(denom) < 1e-6) {
        discard;
    }
    float t = (u_Plane.w - dot(plane_n, origin)) / denom;
    if (!(t >= 0.0) || !(t <= 1.0)) {
        discard;
    }
    vec3 point = origin + ray * t;

    vec4 clip = u_ViewProjection * vec4(point, 1.0);
    if (!(clip.w > 1e-6) || !(clip.z / clip.w >= 0.0) || !(clip.z / clip.w <= 1.0)) {
        discard;
    }
    float grid_depth = clip.z / clip.w;

    float scene_depth = texture(sampler2D(u_SceneDepth, u_Sampler), v_UV).x;
    if (scene_depth < grid_depth) {
        discard;
    }

    vec2 grid_coord = (abs(plane_n.y) > 0.5) ? point.xz : point.xy;
    vec2 derivative = max(abs(fwidth(grid_coord)), vec2(1e-5));
    float pixel_scale = max(derivative.x, derivative.y);

    float minor_vis = smoothstep(1.5, 4.0, u_Params.x / pixel_scale);
    float major_vis = smoothstep(1.5, 4.0, u_Params.y / pixel_scale);
    float minor = gridLine(grid_coord, u_Params.x, derivative) * minor_vis;
    float major = gridLine(grid_coord, u_Params.y, derivative) * major_vis;

    float axis_u = axisLine(grid_coord.x, derivative.x);
    float axis_v = axisLine(grid_coord.y, derivative.y);

    float dist = length(point - u_CameraWorld.xyz);
    float fade = 1.0 - smoothstep(max(u_Params.z, 1.0), max(u_Params.w, u_Params.z + 1.0), dist);

    float grid = clamp(max(minor, major), 0.0, 1.0) * clamp(fade, 0.0, 1.0);
    float axis = clamp(max(axis_u, axis_v), 0.0, 1.0) * clamp(fade, 0.0, 1.0);

    float alpha = clamp(max(grid * 0.85, axis), 0.0, 1.0);
    if (alpha <= 0.002) {
        discard;
    }

    vec3 grid_color = vec3(0.42, 0.44, 0.46);
    vec3 axis_color = (axis_u >= axis_v) ? vec3(0.78, 0.28, 0.28) : vec3(0.28, 0.45, 0.85);
    vec3 rgb = (axis > grid) ? axis_color : grid_color;
    o_Color = vec4(rgb, alpha);
}
