#version 450 core

const int kMaxPointLightCount = 16;
const int kMaxPointLightGeomVertices = kMaxPointLightCount * 6;

layout(location = 0) in vec3 in_position_world_space[];

layout(location = 0) out float out_inv_length;
layout(location = 1) out vec3 out_inv_length_position_view_space;
layout(location = 2) out float out_point_light_radius;

layout(set = 0, binding = 0) uniform PointLightShadowPassUBO {
    uint point_light_count;
    uint _padding_point_light_count_0;
    uint _padding_point_light_count_1;
    uint _padding_point_light_count_2;
    vec4 point_lights_position_and_radius[kMaxPointLightCount];
};

layout(triangles) in;
layout(triangle_strip, max_vertices = kMaxPointLightGeomVertices) out;

void main()
{
    int light_count = int(min(point_light_count, uint(kMaxPointLightCount)));
    for (int point_light_index = 0; point_light_index < light_count; ++point_light_index) {
        vec3 point_light_position = point_lights_position_and_radius[point_light_index].xyz;
        float point_light_radius = point_lights_position_and_radius[point_light_index].w;

        for (int layer_index = 0; layer_index < 2; ++layer_index) {
            for (int vertex_index = 0; vertex_index < 3; ++vertex_index) {
                vec3 position_world_space = in_position_world_space[vertex_index];
                vec3 position_view_space = position_world_space - point_light_position;
                vec3 position_spherical_function_domain = normalize(position_view_space);

                float layer_position_spherical_function_domain_z[2];
                layer_position_spherical_function_domain_z[0] = -position_spherical_function_domain.z;
                layer_position_spherical_function_domain_z[1] = position_spherical_function_domain.z;

                vec4 position_clip;
                position_clip.xy = position_spherical_function_domain.xy;
                position_clip.w = layer_position_spherical_function_domain_z[layer_index] + 1.0;
                position_clip.z = 0.5 * position_clip.w;
                gl_Position = position_clip;

                out_inv_length = 1.0 / length(position_view_space);
                out_inv_length_position_view_space = out_inv_length * position_view_space;
                out_point_light_radius = point_light_radius;

                gl_Layer = layer_index + 2 * point_light_index;
                EmitVertex();
            }
            EndPrimitive();
        }
    }
}