#version 450

layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_UV;

layout(set = 0, binding = 0) uniform MVP {
    mat4 u_Model;
    mat4 u_View;
    mat4 u_Projection;
};

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_UV;

void main() {
    vec4 worldPos = u_Model * vec4(a_Pos, 1.0);
    v_WorldPos = worldPos.xyz;
    v_Normal = mat3(u_Model) * a_Normal;
    v_UV = a_UV;
    gl_Position = u_Projection * u_View * worldPos;
}

#version 450

layout(location = 0) out vec4 o_Color;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec3 v_UV;

layout(set = 0, binding = 1) uniform MaterialUBO {
    float u_Shininess;
    vec3 u_Ambient;
};
layout(set = 0, binding = 2) uniform LightUBO {
    vec3 u_LightDir;
    vec3 u_LightColor;
    vec3 u_CameraPos;
};
layout(set = 0, binding = 3) uniform texture2D u_AlbedoMap;
layout(set = 0, binding = 4) uniform sampler u_Sampler;

void main() {
    vec3 albedo = texture2D(sampler2D(u_AlbedoMap, u_Sampler), v_UV);
    vec3 N = normalize(v_Normal);
    vec3 L = normalize(-u_LightDir);
    vec3 V = normalize(u_CameraPos - v_WorldPos);

    vec3 ambient = u_Ambient * albedo;

    vec3 NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = NdotL * u_LightColor * albedo;

    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float spec = pow(NdotH, u_Shininess);
    vec3 specular = spec * u_LightColor;

    vec3 color = ambient + diffuse + specular;
    o_Color = vec4(color, 1.0);
}