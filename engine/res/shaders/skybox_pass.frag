#version 450 core

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

layout(push_constant) uniform SkyboxPushConstants {
    mat4 u_InvViewProjection;
};

layout(set = 0, binding = 0) uniform textureCube u_SkyboxTexture;
layout(set = 0, binding = 1) uniform texture2D u_SceneDepth;
layout(set = 0, binding = 128) uniform sampler u_Sampler;

void main()
{
    vec2 ndc = vec2(v_UV.x * 2.0 - 1.0, 1.0 - v_UV.y * 2.0);
    vec4 near_world = u_InvViewProjection * vec4(ndc, 0.0, 1.0);
    vec4 far_world = u_InvViewProjection * vec4(ndc, 1.0, 1.0);
    near_world /= max(abs(near_world.w), 1e-6);
    far_world /= max(abs(far_world.w), 1e-6);
    vec3 view_dir = normalize(far_world.xyz - near_world.xyz);

    float scene_depth = texture(sampler2D(u_SceneDepth, u_Sampler), v_UV).r;
    if (scene_depth < 0.99999) {
        discard;
    }

    vec3 sky_color = texture(samplerCube(u_SkyboxTexture, u_Sampler), view_dir).rgb;
    o_Color = vec4(sky_color, 1.0);
}
