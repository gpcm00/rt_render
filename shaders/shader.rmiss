#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shading_language_include : enable
#include "common.glsl"
#include "payload.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
    // From ray tracing in one weekend:
    // https://raytracing.github.io/books/RayTracingInOneWeekend.html
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float a = 0.5 * (dir.y + 1.0);
    payload.color.rgb =
        1.0 * clamp(mix(vec3(0, 0, 0), vec3(0.5, 0.7, 1.0), a), 0.0, 1.0);
    payload.hit = false;
    payload.t = 0.0;
    payload.transmission = 1.0;
}