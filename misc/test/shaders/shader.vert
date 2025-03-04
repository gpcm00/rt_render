#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 4) in vec4 col0;
layout(location = 5) in vec4 col1;
layout(location = 6) in vec4 col2;
layout(location = 7) in vec4 col3;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    mat4 model_transf = mat4(col0, col1, col2, col3);
    gl_Position = ubo.proj * ubo.view * ubo.model * model_transf * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}
