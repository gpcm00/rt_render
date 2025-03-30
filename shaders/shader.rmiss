#version 460
#extension GL_EXT_ray_tracing : require

// layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
// layout(binding = 1, set = 0, rgba8) uniform image2D image;

// layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 0) rayPayloadEXT vec4 payload;

void main()
{
    // payload = vec4(0.0, 0.1, 0.3, 1.0);
    payload = vec4(0.5, 0.0, 0.0, 1.0);
}