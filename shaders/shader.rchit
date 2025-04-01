#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : enable
#extension GL_EXT_scalar_block_layout : enable

struct Vertex {
    vec3  position;
    vec3 normal;
    vec3 color;
    vec2 uvmap;
};

layout(buffer_reference, scalar) buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, scalar) buffer IndexBuffer {
    uint indices[];
};


struct Mesh {
    VertexBuffer vertex;
    IndexBuffer index;
};

layout(scalar, set = 0, binding = 3) buffer Meshes {
  Mesh meshes[];
};


// layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
// layout(binding = 1, set = 0, rgba8) uniform image2D image;

// layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 0) rayPayloadInEXT vec4 payload;
// hitAttributeEXT vec3 attribs;
hitAttributeEXT vec2 bary;
void main()
{

  // vec3 light_pos = vec3(-10.0, 10.0, 0.0); // test light position
  vec3 light_pos = vec3(-4.0, 4.0, -2.0); // test light position

  uint mesh_id = 0; // TODO: get this dynamically
  Mesh mesh = meshes[mesh_id];

  // Retrieve the indices of the triangle
  uint tri = gl_PrimitiveID;

  uint i0 = mesh.index.indices[tri*3];
  uint i1 = mesh.index.indices[tri*3 + 1];
  uint i2 = mesh.index.indices[tri*3 + 2];

  // Retrieve the vertices of the triangle
  Vertex v0 = mesh.vertex.vertices[i0];
  Vertex v1 = mesh.vertex.vertices[i1];
  Vertex v2 = mesh.vertex.vertices[i2];

  vec3 weights = vec3(1.0f - bary.x - bary.y, bary.x, bary.y);
  // vec3 weights = vec3(0.3, 0.3, 0.4);

  vec3 local_normal = normalize(
      v0.normal * weights.x +
      v1.normal * weights.y +
      v2.normal * weights.z
  );

  vec3 local_position = (
      v0.position * weights.x +
      v1.position * weights.y +
      v2.position * weights.z
  );

  vec3 object_color = (
      v0.color * weights.x +
      v1.color * weights.y +
      v2.color * weights.z
  );

  // wack lighting model for debugging

  vec3 position = vec3(gl_ObjectToWorldEXT * vec4(local_position, 1.0));
  vec3 normal = normalize(vec3(local_normal * gl_ObjectToWorldEXT));


  vec3 light_dir = normalize(light_pos - position);
  float ndl = max(dot(normal, light_dir), 0.0);
  vec3 lighting = ndl * vec3(1.0, 1.0, 1.0);
  vec3 ambient = vec3(0.1, 0.1, 0.1);
  // vec3 object_color = vec3(0.0, 1.0, 0.0);
  vec3 color = object_color*(lighting + ambient);

  

  payload = vec4(color, 1.0);
}
