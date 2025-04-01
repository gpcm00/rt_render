#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : enable

struct Vertex {
    vec3  position;
    vec3 normal;
    vec3 color;
    vec2 uvmap;
};

layout(buffer_reference, std430) buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, std430) buffer IndexBuffer {
    uint indices[];
};


struct Mesh {
    VertexBuffer vertex;
    IndexBuffer index;
};

layout(std430, set = 0, binding = 3) buffer Meshes {
  Mesh meshes[];
};


// layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
// layout(binding = 1, set = 0, rgba8) uniform image2D image;

// layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 0) rayPayloadInEXT vec4 payload;
// hitAttributeEXT vec3 attribs;

void main()
{
  payload = vec4(0.0, 1.0, 0.0, 1.0);
}
