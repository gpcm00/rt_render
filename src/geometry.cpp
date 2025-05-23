#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION

#include <geometry/geometry.hpp>

namespace fs = std::filesystem;

TextureMap::TextureMap(tinygltf::Image &image, TextureType texture_type)
    : texture_type(texture_type) {
    map = image.image;
    w = image.width;
    h = image.height;
    c = image.component;

    if (c == 3) {
        // Convert to RGBA
        std::vector<unsigned char> rgba_map(w * h * 4);
        for (int i = 0; i < w * h; ++i) {
            rgba_map[i * 4] = map[i * 3];
            rgba_map[i * 4 + 1] = map[i * 3 + 1];
            rgba_map[i * 4 + 2] = map[i * 3 + 2];
            rgba_map[i * 4 + 3] = 255; // Set alpha to 255 (opaque)
        }
        map = std::move(rgba_map);
        c = 4;
    }
}

TextureMap::TextureMap(glm::vec4 value, TextureType texture_type)
    : texture_type(texture_type) {

    w = 1;
    h = 1;
    c = 4;
    map.resize(w * h * c);

    if (texture_type == TextureType::normalTexture) {
        unsigned char out_val = 255;
        unsigned char zero_val = 128;
        map[0] = zero_val;
        map[1] = zero_val;
        map[2] = out_val;
        map[3] = zero_val;
    } else {
        value *= 255.0f;
        map[0] = static_cast<unsigned char>(value.r);
        map[1] = static_cast<unsigned char>(value.g);
        map[2] = static_cast<unsigned char>(value.b);
        map[3] = static_cast<unsigned char>(value.a);
    }
}

void TextureMap::free_texture_map() {}

static glm::mat4 get_node_transform(const tinygltf::Node &node) {
    glm::mat4 transform = glm::mat4(1.0f); // Identity matrix

    if (!node.matrix.empty()) {
        transform = glm::make_mat4(node.matrix.data());
    } else {
        glm::vec3 translation(0.0f);
        glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
        glm::vec3 scale(1.0f);

        if (!node.translation.empty()) {
            translation = glm::vec3(node.translation[0], node.translation[1],
                                    node.translation[2]);
        }

        if (!node.rotation.empty()) {
            rotation = glm::quat(node.rotation[3], node.rotation[0],
                                 node.rotation[1], node.rotation[2]);
        }

        if (!node.scale.empty()) {
            scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
        }

        transform = glm::translate(glm::mat4(1.0f), translation) *
                    glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
    }

    return transform;
}

static glm::mat4 get_node_global_transform(
    const tinygltf::Node &node,
    std::unordered_map<const tinygltf::Node *, const tinygltf::Node *>
        &parents) {

    auto transform = get_node_transform(node);
    const tinygltf::Node *current = &node;
    while (parents.find(current) != parents.end()) {
        current = parents[current];
        const auto parent_transform = get_node_transform(*current);
        transform = parent_transform * transform;
    }

    return transform;
}

static size_t populate_vertex_data(tinygltf::Model &model,
                                   const tinygltf::Primitive &primitive,
                                   std::vector<Vertex> &vertices) {
    const tinygltf::Accessor &posAccessor =
        model.accessors[primitive.attributes.at("POSITION")];
    const tinygltf::BufferView &posView =
        model.bufferViews[posAccessor.bufferView];
    const tinygltf::Buffer &posBuffer = model.buffers[posView.buffer];

    const float *posData = reinterpret_cast<const float *>(
        &posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);

    const float *normalData = nullptr;
    if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
        const tinygltf::Accessor &normalAccessor =
            model.accessors[primitive.attributes.at("NORMAL")];
        const tinygltf::BufferView &normalView =
            model.bufferViews[normalAccessor.bufferView];
        const tinygltf::Buffer &normalBuffer = model.buffers[normalView.buffer];

        normalData = reinterpret_cast<const float *>(
            &normalBuffer
                 .data[normalView.byteOffset + normalAccessor.byteOffset]);
    }

    const float *textureData = nullptr;
    if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
        const tinygltf::Accessor &textureAccessor =
            model.accessors[primitive.attributes.at("TEXCOORD_0")];
        const tinygltf::BufferView &textureView =
            model.bufferViews[textureAccessor.bufferView];
        const tinygltf::Buffer &textureBuffer =
            model.buffers[textureView.buffer];

        textureData = reinterpret_cast<const float *>(
            &textureBuffer
                 .data[textureView.byteOffset + textureAccessor.byteOffset]);
    }

    const float *colorData = nullptr;
    if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
        const tinygltf::Accessor &colorAccessor =
            model.accessors[primitive.attributes.at("COLOR_0")];
        const tinygltf::BufferView &colorView =
            model.bufferViews[colorAccessor.bufferView];
        const tinygltf::Buffer &colorBuffer = model.buffers[colorView.buffer];

        colorData = reinterpret_cast<const float *>(
            &colorBuffer.data[colorView.byteOffset + colorAccessor.byteOffset]);
    }

    size_t i = 0;
    for (; i < posAccessor.count; i++) {
        Vertex vertex;
        vertex.position =
            glm::vec3(posData[i * 3], posData[i * 3 + 1], posData[i * 3 + 2]);

        if (normalData != nullptr) {
            vertex.normal = glm::vec3(normalData[i * 3], normalData[i * 3 + 1],
                                      normalData[i * 3 + 2]);
        } else {
            vertex.normal = glm::vec3(0.0f);
        }

        if (textureData != nullptr) {
            vertex.uvmap =
                glm::vec2(textureData[i * 2], textureData[i * 2 + 1]);
        } else {
            vertex.uvmap = glm::vec2(0.0f);
        }
        if (colorData != nullptr) {
            vertex.color = glm::vec3(colorData[i * 3], colorData[i * 3 + 1],
                                     colorData[i * 3 + 2]);
        } else {
            vertex.color = glm::vec3(1.0f);
        }

        vertices.push_back(vertex);
    }

    return i;
}

static size_t populate_index_data(tinygltf::Model &model,
                                  const tinygltf::Primitive &primitive,
                                  std::vector<uint32_t> &indices) {
    size_t i = 0;
    if (primitive.indices >= 0) {
        const tinygltf::Accessor &indexAccessor =
            model.accessors[primitive.indices];
        const tinygltf::BufferView &indexView =
            model.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer &indexBuffer = model.buffers[indexView.buffer];

        const void *indexData =
            &indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset];

        for (; i < indexAccessor.count; i++) {
            if (indexAccessor.componentType ==
                TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                indices.push_back(
                    reinterpret_cast<const uint16_t *>(indexData)[i]);
            } else if (indexAccessor.componentType ==
                       TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                indices.push_back(
                    reinterpret_cast<const uint32_t *>(indexData)[i]);
            } else {
                std::cerr << "Unsupported index type: "
                          << indexAccessor.componentType << std::endl;
                return 0;
            }
        }
    }
    return i;
}

Scene::Scene(const std::string &filename) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    fs::path base_dir = fs::path(filename).parent_path();

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);

    if (!warn.empty())
        std::cout << "Warning: " << warn << std::endl;
    if (!ret) {
        std::cerr << "Failed to load GLTF file: " << err << std::endl;
        return;
    }

    std::cout << "Successfully loaded GLTF: " << filename << std::endl;

    // Build map of parents
    std::unordered_map<const tinygltf::Node *, const tinygltf::Node *> parents;
    for (size_t i = 0; i < model.nodes.size(); i++) {
        for (auto &child : model.nodes[i].children) {
            const tinygltf::Node *child_node = &model.nodes[child];
            parents[child_node] = (const tinygltf::Node *)&model.nodes[i];
        }
    }

    geometries.resize(model.meshes.size());

    size_t mesh_i = 0;
    primitive_id = 0;
    for (const auto &mesh : model.meshes) {
        for (auto &primitive : mesh.primitives) {
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                std::cerr << "Unsupported primitive mode: " << primitive.mode
                          << std::endl;
                continue;
            }

            // add a new primitive to the mesh
            geometries[mesh_i].primitives.push_back({});
            auto &p = geometries[mesh_i].primitives.back();
            size_t n_vertices =
                populate_vertex_data(model, primitive, p.vertices);
            size_t n_indices = populate_index_data(model, primitive, p.indices);

            p.primitive_id = primitive_id++;
            p.material_index = primitive.material;

            std::cout << "Mesh[" << mesh_i << "] Primitive:\n";
            std::cout << "\tVertices:" << n_vertices << std::endl;
            std::cout << "\tIndices:" << n_indices << std::endl;
        }
        geometries[mesh_i].mesh_id = mesh_i;
        mesh_i++;
    }

    objects.clear();
    size_t obj_i = 0;
    for (auto &node : model.nodes) {
        if (node.mesh >= 0) {
            objects.push_back({&geometries[node.mesh], get_node_transform(node),
                               get_node_global_transform(node, parents)});
            obj_i++;
        }
    }

    size_t mat_i = 0;
    for (auto &material : model.materials) {
        materials.push_back(Material(material, model, base_dir));
        mat_i++;
    }

    std::cout << "Number of meshes: " << mesh_i << std::endl;
    std::cout << "Number of objects: " << obj_i << std::endl;
    std::cout << "Number of materials: " << mat_i << std::endl;
}
