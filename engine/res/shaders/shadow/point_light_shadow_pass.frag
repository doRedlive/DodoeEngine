#version 450 core

layout(location = 0) in float in_inv_length;
layout(location = 1) in vec3 in_inv_length_position_view_space;
layout(location = 2) in float in_point_light_radius;

void main()
{
    vec3 position_view_space = in_inv_length_position_view_space / in_inv_length;
    float point_light_radius = max(in_point_light_radius, 0.001);
    float ratio = length(position_view_space) / point_light_radius;
    gl_FragDepth = ratio;
}