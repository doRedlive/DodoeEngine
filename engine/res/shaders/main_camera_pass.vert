#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Normal;
layout(location = 2) in vec2 a_UV;
layout(location = 3) in vec4 a_Model0;
layout(location = 4) in vec4 a_Model1;
layout(location = 5) in vec4 a_Model2;
layout(location = 6) in vec4 a_Model3;

layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec2 v_UV;
layout(location = 2) out vec3 v_WorldPosition;
layout(location = 3) flat out uint v_TexIndex;

layout(set = 0, binding = 256) uniform MainCameraPassUBO {
    mat4 u_ViewProjection;
    ivec4 u_DrawData;
    vec4 u_MaterialData;
};

void main()
{
    mat4 model = mat4(a_Model0, a_Model1, a_Model2, a_Model3);
    mat3 normal_matrix = transpose(inverse(mat3(model)));
    vec4 world_position = model * vec4(a_Position, 1.0);
    v_Normal = normalize(normal_matrix * a_Normal.xyz);
    v_UV = a_UV;
    v_WorldPosition = world_position.xyz;
    v_TexIndex = uint(u_DrawData.x);
    gl_Position = u_ViewProjection * world_position;
}
