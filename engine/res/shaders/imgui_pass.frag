#version 450

layout(set = 0, binding = 0) uniform texture2D uTexture;
layout(set = 0, binding = 128) uniform sampler uSampler;

layout(location = 0) in vec2 v_UV;
layout(location = 1) in vec4 v_Color;

layout(location = 0) out vec4 o_Color;

void main() {
    vec4 sampled_color = texture(sampler2D(uTexture, uSampler), v_UV);
    o_Color = v_Color * sampled_color;
}
