#version 450

layout(push_constant) uniform PushConstants {
    vec2 uInvDisplaySize;
    vec2 uDisplayPos;
} pc;

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in vec4 a_Color;

layout(location = 0) out vec2 v_UV;
layout(location = 1) out vec4 v_Color;

void main() {
    v_UV = a_UV;
    v_Color = a_Color;
    vec2 ndc;
    vec2 pos = a_Position - pc.uDisplayPos;
    ndc.x = pos.x * pc.uInvDisplaySize.x * 2.0 - 1.0;
    ndc.y = 1.0 - pos.y * pc.uInvDisplaySize.y * 2.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
