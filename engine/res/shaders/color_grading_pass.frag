#version 450

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

layout(push_constant) uniform ColorGradingPushConstants {
    vec4 u_Params;
};

layout(set = 0, binding = 0) uniform texture2D u_InputTexture;
layout(set = 0, binding = 128) uniform sampler u_Sampler;

void main()
{
    vec3 color = texture(sampler2D(u_InputTexture, u_Sampler), v_UV).rgb;
    float exposure = u_Params.x;
    float saturation = u_Params.y;
    float contrast = u_Params.z;
    float gamma = max(u_Params.w, 0.0001);

    color *= exposure;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, saturation);
    color = (color - 0.5) * contrast + 0.5;
    color = pow(max(color, vec3(0.0)), vec3(1.0 / gamma));
    o_Color = vec4(clamp(color, 0.0, 1.0), 1.0);
}