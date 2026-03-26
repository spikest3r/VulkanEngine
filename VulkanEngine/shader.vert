#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor; // Used as normal in your fragment shader logic
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;

// Simplified UBO: No array, just the data for ONE object.
// The C++ dynamic offset handles which object's data is visible here.
layout(binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 1.0);
    
    // Pass the normal (inColor) to the fragment shader
    // Note: In a real app, you'd want to transform normals by the model matrix too.
    fragNormal = inColor; 
    fragTexCoord = inTexCoord;
}