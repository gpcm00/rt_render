#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

// layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
// layout(binding = 1, set = 0, rgba8) uniform image2D image;

// layout(location = 0) rayPayloadInEXT vec3 hitValue;
struct RayPayload {
  vec3 rayOrigin;
  vec3 rayDirection;
  int level;
  vec4 color;
  vec3 contribution;
};

layout(location = 0) rayPayloadEXT RayPayload payload;
void main()
{
    // payload = vec4(0.0, 0.1, 0.3, 1.0);
    payload.rayDirection = vec3(0.0);
    payload.rayOrigin = vec3(0.0);
    payload.contribution = vec3(0.0);
    payload.color = vec4(0.0);
    // payload = vec4(0.2, 0.2, 0.5, 1.0);
}