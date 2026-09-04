#version 450

layout(location = 0) out vec2 v_UV;

vec2 NDC[6] = vec2[](
    vec2(-0.8, -0.8),
    vec2( 0.8, -0.8),
    vec2( 0.8,  0.8),
    vec2(-0.8, -0.8),
    vec2( 0.8,  0.8),
    vec2(-0.8,  0.8)
);

void main()
{
    vec2 pos = NDC[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);
    v_UV = vec2(pos.x * 0.5 + 0.5, 0.5 - pos.y * 0.5);
}
