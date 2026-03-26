#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

void main() {
    // 1. Normalize the input normal
    vec3 N = normalize(fragNormal);

    // 2. Light direction (adjust this to change shadow direction)
    vec3 L = normalize(vec3(0.5, 0.5, 1.0));

    // 3. Simple Lambertian diffuse + Ambient
    float diffuse = max(dot(N, L), 0.0);
    float ambient = 0.25;
    float lightIntensity = clamp(diffuse + ambient, 0.0, 1.0);

    // 4. Sample texture and apply light
    vec4 texColor = texture(texSampler, fragTexCoord);
    
    // Final output: RGB scaled by light, Alpha kept from texture
    outColor = vec4(texColor.rgb * lightIntensity, texColor.a);
}