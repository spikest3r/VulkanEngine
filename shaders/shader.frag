#version 450
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

// set=1: texture descriptor set (lives on the Texture object)
layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    vec3     lightPos;
    float    ambient;
    vec3     lightColor;
    uint     unlit;
} pc;

void main() {
    vec4 texColor = texture(texSampler, fragTexCoord);
    if (pc.unlit == 1u) {
        outColor = texColor;
        return;
    }
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(pc.lightPos);
    float diffuse        = max(dot(N, L), 0.0);
    float lightIntensity = clamp(diffuse + pc.ambient, 0.0, 1.0);
    outColor = vec4(texColor.rgb * lightIntensity * pc.lightColor, texColor.a);
}