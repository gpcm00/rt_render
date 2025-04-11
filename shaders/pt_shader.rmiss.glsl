#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

struct RayPayload {
    vec3 rayOrigin;
    vec3 rayDirection;
    int level;
    vec4 color;
    vec3 contribution;
};

layout(location = 0) rayPayloadEXT RayPayload payload;
void main() {
    payload.rayDirection = vec3(0.0);
    payload.rayOrigin = vec3(0.0);
    payload.contribution = vec3(0.0);
    payload.color = vec4(0.0);
}