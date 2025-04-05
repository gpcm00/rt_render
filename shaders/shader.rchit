#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : enable
#extension GL_EXT_scalar_block_layout : enable

#extension GL_ARB_shading_language_include : enable
#include "payload.glsl"
#include "common.glsl"
#include "pbr.glsl"

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
    
} camera;

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

struct Material {
    float transmission;
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

layout(scalar, set = 0, binding = 5) buffer Materials {
    Material materials[];
};

layout(set = 0, binding = 6) uniform sampler2D base_color_tex[];
layout(set = 0, binding = 7) uniform sampler2D normal_tex[];
layout(set = 0, binding = 8) uniform sampler2D metalness_roughness_tex[];
layout(set = 0, binding = 9) uniform sampler2D emissive_tex[];
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
// layout(binding = 1, set = 0, rgba8) uniform image2D image;

// layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 0) rayPayloadInEXT RayPayload payload;

hitAttributeEXT vec2 bary;
void main()
{
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

    vec3 local_normal = normalize(v0.normal * weights.x + v1.normal * weights.y + v2.normal * weights.z);
    vec3 local_position = v0.position * weights.x + v1.position * weights.y + v2.position * weights.z;
    vec3 object_color = v0.color * weights.x + v1.color * weights.y + v2.color * weights.z;
    vec2 uv = v0.uvmap * weights.x + v1.uvmap * weights.y + v2.uvmap * weights.z;
    
    
    // Adapted from https://stackoverflow.com/questions/35723318/getting-the-tangent-for-a-object-space-to-texture-space
    // and https://learnopengl.com/Advanced-Lighting/Normal-Mapping
    vec3 delta_v1 = v1.position-v0.position;
    vec3 delta_v2 = v2.position-v0.position;
    
    vec2  delta_uv1 = v1.uvmap-v0.uvmap;
    vec2  delta_uv2 = v2.uvmap-v0.uvmap;
    float r = 1.0f / (delta_uv1.x * delta_uv2.y - delta_uv1.y * delta_uv2.x);
    vec3  local_tangent = (delta_v1*delta_uv2.y - delta_v2 * delta_uv1.y) * r;




    vec3 position = vec3(gl_ObjectToWorldEXT * vec4(local_position, 1.0));
    vec3 normal = normalize(vec3(local_normal * gl_WorldToObjectEXT));
    vec3 tangent = normalize(vec3(local_tangent * gl_WorldToObjectEXT));
    vec3 bitangent = normalize(cross(normal, tangent)); // missing tangent.w
    mat3 normal_matrix = mat3(tangent, bitangent, normal);

    // vec3 light_pos = vec3(50, 50, 0); // test light position
    // vec3 light_dir = normalize(light_pos - position);
    // vec3 light_color = 250.0*vec3(1.0, 1.0, 1.0);
    // float ndl = max(dot(normal, light_dir), 0.0);
    // vec3 lighting = ndl * light_color;
    //   vec3 ambient = vec3(0.1, 0.1, 0.1);
    uint texture_id = mesh.material_id;
    vec3 base_color = texture(nonuniformEXT(base_color_tex[texture_id]), uv).xyz;
    base_color = decode_sRGB(base_color);
    vec3 normal_map = texture(nonuniformEXT(normal_tex[texture_id]), uv).xyz;
    normal = normalize(normal_matrix * (normal_map * 2.0 - 1.0));
    vec3 metalness_roughness = texture(nonuniformEXT(metalness_roughness_tex[texture_id]), uv).rgb;
    // metalness should be B channel and roughness should be G channel
    // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_material_pbrmetallicroughness_metallicroughnesstexture
    float metalness = metalness_roughness.b;
    float roughness = metalness_roughness.g;

    float transmission = materials[texture_id].transmission;

    vec3 emissive = texture(nonuniformEXT(emissive_tex[texture_id]), uv).xyz;
    emissive = decode_sRGB(emissive);
    
    // Debugging
    // object_color = normal_map;
    // object_color =  vec3(0.0, metalness_roughness);
    // object_color = emissive;

    object_color = base_color;

    float reflectance = 0.5f;
    vec3 f0 = 0.16 * reflectance * reflectance * (1.0 - metalness) + base_color * metalness;
    // vec3 view = normalize(camera.position.xyz - position);
    vec3 view = -normalize(gl_WorldRayDirectionEXT);  // towards the camera
    // float light_distance = length(light_pos - position);
    // float light_attenuation = 1.0 / (1.0 + light_distance * light_distance);

    // vec3 color = BRDF_Filament(normal, light_dir, view, roughness, metalness, f0, base_color, light_attenuation*light_color);
    // color += emissive;

    // let's also trace a ray
    vec3 indirect_light = vec3(0.0);
    vec3 next_ray_dir = vec3(0.0);
    // vec3 next_ray_dir = normalize(reflect(gl_WorldRayDirectionEXT, normal));
// //a
  vec3 random = random_pcg3d(payload.sample_id*uvec3(gl_LaunchIDEXT.xy, payload.depth));
  vec3 contribution = vec3(0.0);
{    
  float e0 = random.x;
  float e1 = random.y;
  float a = roughness * roughness;
  float a2 = a*a;
  float theta = acos(sqrt((1.0 - e0) / ((a2 - 1.0) * e0 + 1.0)));
  float phi = 2*PI * e1;
  vec2 xi = vec2(phi, theta);
  
  // vec3 rayDirection = importanceSamplingGGXD(normal, payload.rayDirection, light_dir, color, roughness, random, contribution);
  next_ray_dir = importanceSampleGGX(normal, view, roughness, xi, f0, contribution);
}
    vec3 normalized_contribution = normalize(contribution);
    next_ray_dir = normalize(next_ray_dir);
    uint depth = payload.depth;
    payload.depth += 1;

    const uint max_depth = 5; // make this configurable later
    float indirect_dist = 0.0;
    vec3 color = vec3(0.0);

    // For transmission
    float eta = 1.5; // 1.5 glass

    // Bounce lighting
    if (payload.depth < max_depth) {
        // payload.depth += 1;

        // if (transmission > 0.0 && (random.x > 0.01)) {
        if (transmission > random.x) {
            // the code below for transmission doesn't really work

            
            // cheap trick: let's assume back face is always in glass
            vec3 n = normal;
            if (dot(normal, gl_WorldRayDirectionEXT) < 0.0 ) {
                // inside glass
                next_ray_dir = normalize(refract(gl_WorldRayDirectionEXT, normal, 1.0/eta));
            }
            else {
                next_ray_dir = normalize(refract(gl_WorldRayDirectionEXT, -normal, eta));
                n = -normal;
            }
            vec3 ray_origin = position;
            // vec3 ray_origin = position;
            // next_ray_dir = -normal;
            Ray ray = Ray(ray_origin, next_ray_dir);

            traceRayEXT(
                topLevelAS, 
                gl_RayFlagsOpaqueEXT, 
                0xFF, // mask
                0, // sbt offset
                0, // sbt stride
                0, // miss index
                ray.origin, // ray origin
                camera.rmin, // ray min
                ray.direction, // ray direction, 
                camera.rmax, //ray max
                0 // ray payload
            );

            vec3 transmission_color = payload.color.xyz;
            float dist = payload.t;
            float atten = 1.0;
            if (payload.hit) {
                atten = 1.0 / (1.0 + dist * dist);
            }
            // vec3 trans_dir = next_ray_dir;
            // We need a proper BTDF?

        //    vec3 h = normalize(view + normal);
        //     // float LoH = clamp(dot(l, h), 0.0, 1.0);
        //     float u = clamp(dot(-view, normal), 0.0, 1.0);
        //     vec3 f = F_Schlick(u, f0);

            color += atten*transmission_color;
            // color += (transmission)*transmission_color;
            // color += (transmission)*BRDF_Filament(normal, trans_dir, view, roughness, metalness, f0, base_color, atten*transmission_color);
        }
        else {

        Ray ray = Ray(position, next_ray_dir);

        traceRayEXT(
                topLevelAS, 
                gl_RayFlagsOpaqueEXT, 
                0xFF, // mask
                0, // sbt offset
                0, // sbt stride
                0, // miss index
                ray.origin, // ray origin
                camera.rmin, // ray min
                ray.direction, // ray direction, 
                camera.rmax, //ray max
                0 // ray payload
            );
            // payload.depth -= 1;
            indirect_light = payload.color.xyz;
            indirect_dist = payload.t;
            
                    // vec3 indirect_dir = -normalize(reflect(view, normal));
            vec3 indirect_dir = next_ray_dir;
            // indirect_light = vec3(1.0, 1.0, 1.0);
            // float indirect_length = length(indirect_light);
            float indirect_attenuation = 1.0;
            if (payload.hit) {
                indirect_attenuation = 1.0 / (1.0 + indirect_dist * indirect_dist);
            }

            // vec3 h = normalize(view + normal);
            // // float LoH = clamp(dot(l, h), 0.0, 1.0);
            // float u = clamp(dot(-view, normal), 0.0, 1.0);
            // vec3 f = F_Schlick(u, f0);

            // color += (1.0-transmission)*BRDF_Filament(normal, indirect_dir, view, roughness, metalness, f0, base_color, indirect_attenuation*indirect_light);
            color += BRDF_Filament(normal, indirect_dir, view, roughness, metalness, f0, base_color, indirect_attenuation*indirect_light);
            // color += normalized_contribution*BRDF_Filament(normal, indirect_dir, view, roughness, metalness, f0, base_color, indirect_attenuation*indirect_light);


        }
    }



    // feeble attempt at direct lighting
    if (depth < max_depth && transmission <= random.x) {
        vec3 light_pos = 3*vec3(3, 3, 3); // test light position
        vec3 light_color = 300.0*vec3(1.0, 1.0, 1.0);

        vec3 to_light = normalize(light_pos - position);
        float light_distance = length(light_pos - position);

        // uint last_depth = depth;
        // payload.depth = max_depth; // want it to not shoot new rays after this one
        payload.depth = depth+1;
        payload.hit = true;
        // should use an occlusion group for this, but not enough time
        traceRayEXT(
                topLevelAS, 
                gl_RayFlagsOpaqueEXT, 
                0xFF, // mask
                0, // sbt offset
                0, // sbt stride
                0, // miss index
                position, // ray origin
                camera.rmin, // ray min
                to_light, // ray direction, 
                camera.rmax, //ray max
                0 // ray payload
            );

            payload.depth = depth; // reset

            if (!payload.hit || payload.t >= light_distance) {
                // then add direct lighting
                float light_attenuation = 1.0 / (1.0 + light_distance * light_distance);
                color += BRDF_Filament(normal, to_light, view, roughness, metalness, f0, base_color, light_attenuation*light_color);
            }
            else {
                // add transmitted lighting
                float light_distance = payload.t;
                float light_attenuation = 1.0 / (1.0 + light_distance * light_distance);
                // color += light_attenuation*payload.transmission*payload.color.xyz;
                color += BRDF_Filament(normal, to_light, view, roughness, metalness, f0, base_color, payload.transmission*light_attenuation*payload.color.xyz);
            }

    }
    
    
    color += emissive*1.0;

    payload.color = vec4(color, 1.0);
    // payload.color = vec4(1.0, 1.0, 1.0, 1.0);

    // payload.color = vec4(color + indirect_light, 1.0);
    payload.hit = true;
    payload.t = gl_HitTEXT;
    payload.transmission = transmission;
}
