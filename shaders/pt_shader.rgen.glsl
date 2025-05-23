#version 460
#extension GL_EXT_ray_tracing : require

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
// Change format and image setup code if needed
layout(binding = 1, set = 0, rgba8) uniform image2D image;

// Be wary of alignment
layout(std140, binding = 2, set = 0) uniform Camera {
    vec4 position;
    vec4 direction;
    vec4 up;
    vec4 right;
    float fov;
    float rmin;
    float rmax;
    float aspect_ratio;
}
camera;

struct RayPayload {
    vec3 rayOrigin;
    vec3 rayDirection;
    int level;
    vec4 color;
    vec3 contribution;
};

layout(location = 0) rayPayloadEXT RayPayload payload;

struct Ray {
    vec3 origin;
    vec3 direction;
};

// Computes the ray direction based on the camera parameters and the pixel
// coordinate
Ray compute_perspective_ray(vec2 uv, vec3 position, vec3 direction, vec3 up,
                            vec3 right, float fov, float aspect_ratio) {
    float scale = tan(fov * 0.5);
    vec3 ray_direction = normalize((uv.x - 0.5) * aspect_ratio * scale * right -
                                   (uv.y - 0.5) * scale * up + direction);

    return Ray(position, ray_direction);
}

void main() {

    payload = vec4(0.0, 0.0, 0.0, 1.0);

    // Get pixel coordinates
    uvec2 pixel = gl_LaunchIDEXT.xy;
    uvec2 resolution = gl_LaunchSizeEXT.xy;

    // Normalize pixel coords
    vec2 uv = (vec2(pixel) + 0.5) / vec2(resolution);

    Ray ray = compute_perspective_ray(
        uv, camera.position.xyz, camera.direction.xyz, camera.up.xyz,
        camera.right.xyz, camera.fov, camera.aspect_ratio);

    payload.rayOrigin = ray.origin;
    payload.rayDirection = ray.direction;
    payload.level = 0;
    vec4 pixelColor = vec3(0.0);

    const int maxLevel = 5;
    int level = 0;
    vec3 contribution = vec2(1.0);

    while (length(payload.rayDirection) > 0.1 && level < maxLevel &&
           length(contribution) > 0.001) {
        payload.level = level;
        traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT,
                    0xFF,                 // mask
                    0,                    // sbt offset
                    0,                    // sbt stride
                    0,                    // miss index
                    payload.rayOrigin,    // ray origin
                    camera.rmin,          // ray min
                    payload.rayDirection, // ray direction,
                    camera.rmax,          // ray max
                    0                     // ray payload
        );
        pixelColor += payload.color * contribution;
        contribution *= payload.contribution;
        level++;
    }

    simageStore(image, ivec2(gl_LaunchIDEXT.xy), pixelColor);
}
