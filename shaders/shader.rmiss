#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shading_language_include : enable
#include "payload.glsl"
#include "common.glsl"

// layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
// layout(binding = 1, set = 0, rgba8) uniform image2D image;

// layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 0) rayPayloadInEXT RayPayload payload;

// const vec3 background_light = 2.0*vec3(0.2, 0.2, 0.5);
// const vec3 background_light = 20.0*vec3(0.2, 0.2, 0.2);


void main()
{
    // payload = vec4(0.0, 0.1, 0.3, 1.0);
    // payload.color = vec4(0.2, 0.2, 0.5, 1.0);
    // payload.color = vec4(background_light, 1.0);

    // From ray tracing in one weekend: https://raytracing.github.io/books/RayTracingInOneWeekend.html
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float a = 0.5*(dir.y + 1.0);
    // payload.color.rgb = clamp(mix(vec3(1.0, 0.7, 0.5), vec3(0.5, 0.7, 1.0), a), 0.0, 1.0);
    payload.color.rgb = clamp(mix(vec3(0.5, 0.5, 0.5), vec3(0.5, 0.7, 1.0), a), 0.0, 1.0);
    // payload.color.xyz = vec3(0.5,0.5,0.5);
    // payload.color.rgb = vec3(1.0);
    // payload.color = vec4( mix(vec3(1.0, 0.7, 0.5), vec3(0.5, 0.7, 1.0), a), 1.0);
    // payload.color.xyz = vec3(0.0);

    // payload.depth +=  1;
    payload.hit = false;
    payload.t = 0.0;
    // payload = vec4(my_color, 1.0);
}