#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in uint a_TexIndex;
layout(location = 4) in vec4 i_Data1;
layout(location = 5) in vec4 i_Data2;
layout(location = 6) in vec4 i_Data3;
layout(location = 7) in vec4 i_Data4;

layout(location = 0) out vec2 v_UV;
layout(location = 1) out vec4 v_Color;
layout(location = 2) flat out uint v_TexIndex;

layout(set = 0, binding = 256) uniform SpriteCameraUBO
{
    mat4 u_ViewProjection;
};

void main()
{
    vec2 position = i_Data1.xy;
    vec2 scale = i_Data1.zw;
    float rotation = i_Data2.x;
    uint texIndex = floatBitsToUint(i_Data2.z);
    vec2 uvMin = i_Data3.xy;
    vec2 uvMax = i_Data3.zw;
    uint packedColor = floatBitsToUint(i_Data4.x);

    float c = cos(rotation);
    float s = sin(rotation);
    mat2 rotationMatrix = mat2(c, -s, s, c);
    vec2 worldPosition = rotationMatrix * (a_Position.xy * scale) + position;

    v_UV = mix(uvMin, uvMax, a_UV);
    v_Color = vec4(
        float((packedColor >> 0u) & 0xFFu) / 255.0,
        float((packedColor >> 8u) & 0xFFu) / 255.0,
        float((packedColor >> 16u) & 0xFFu) / 255.0,
        float((packedColor >> 24u) & 0xFFu) / 255.0);
    v_TexIndex = texIndex;
    gl_Position = u_ViewProjection * vec4(worldPosition, 0.0, 1.0);
}
