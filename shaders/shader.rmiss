#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shading_language_include : enable
#include "common.glsl"

// layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
// layout(binding = 1, set = 0, rgba8) uniform image2D image;

// layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 0) rayPayloadInEXT vec4 payload;

void main()
{
    // payload = vec4(0.0, 0.1, 0.3, 1.0);
    payload = vec4(0.2, 0.2, 0.5, 1.0);
    // payload = vec4(my_color, 1.0);
}