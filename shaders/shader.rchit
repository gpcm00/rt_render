#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : enable
#extension GL_EXT_scalar_block_layout : enable

struct Vertex {
    vec3 position;
    float padding0;
    vec3 normal;
    float padding1;
    vec3 color;
    float padding2;
    vec2 uvmap;
    vec2 padding3;
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
    uint material_id;
};

layout(scalar, set = 0, binding = 3) buffer Meshes {
  Mesh meshes[];
};

struct Instance {
    uint mesh_id;
};

layout(scalar, set = 0, binding = 4) buffer Instances {
    Instance instances[];
};

layout(set = 0, binding = 5) uniform sampler2D base_color_tex[];
layout(set = 0, binding = 6) uniform sampler2D normal_tex[];
layout(set = 0, binding = 7) uniform sampler2D metalness_roughness_tex[];
layout(set = 0, binding = 8) uniform sampler2D emissive_tex[];
// layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
// layout(binding = 1, set = 0, rgba8) uniform image2D image;

// layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 0) rayPayloadInEXT vec4 payload;
// hitAttributeEXT vec3 attribs;
hitAttributeEXT vec2 bary;
void main()
{

  // vec3 light_pos = vec3(-10.0, 10.0, 0.0); // test light position
//   vec3 light_pos = vec3(-4.0, 4.0, -2.0); // test light position
//   vec3 light_pos = vec3(40.0, 40.0, -20.0); // test light position
  vec3 light_pos = vec3(0, 40, 0); // test light position
  uint mesh_id = instances[gl_InstanceCustomIndexEXT].mesh_id;
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

  vec2 uv = (
      v0.uvmap * weights.x +
      v1.uvmap * weights.y +
      v2.uvmap * weights.z
  );

  // wack lighting model for debugging

    vec3 position = vec3(gl_ObjectToWorldEXT * vec4(local_position, 1.0));
    vec3 normal = normalize(vec3(local_normal * gl_WorldToObjectEXT));
    // mat3 normal_matrix = transpose(mat3(gl_WorldToObjectEXT));
    // vec3 normal = normalize(normal_matrix * local_normal);

  vec3 light_dir = normalize(light_pos - position);
  float ndl = max(dot(normal, light_dir), 0.0);
  vec3 lighting = ndl * vec3(1.0, 1.0, 1.0);
  vec3 ambient = vec3(0.1, 0.1, 0.1);
  // vec3 object_color = vec3(0.0, 1.0, 0.0);
  uint texture_id = mesh.material_id;
  // uint texture_id = 0;
  vec3 base_color = texture(nonuniformEXT(base_color_tex[texture_id]), uv).xyz;
  vec3 normal_map = texture(nonuniformEXT(normal_tex[texture_id]), uv).xyz;
  vec3 metalness_roughness = texture(nonuniformEXT(metalness_roughness_tex[texture_id]), uv).rgb;
  // metalness should be B channel and roughness should be G channel
  // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_material_pbrmetallicroughness_metallicroughnesstexture
  float metalness = metalness_roughness.b;
  float roughness = metalness_roughness.g;

  vec3 emissive = texture(nonuniformEXT(emissive_tex[texture_id]), uv).xyz;

  // Debugging
  // object_color = normal_map;
  // object_color =  vec3(0.0, metalness_roughness);
  // object_color = emissive;

  object_color = base_color;

  vec3 color = clamp(object_color*(lighting + ambient), 0.0, 1.0);


  payload = vec4(color, 1.0);
}
