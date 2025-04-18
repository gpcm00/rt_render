#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <cstdlib>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <tiny_gltf.h>

// #include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

#include "geometry.hpp"

namespace fs = std::filesystem;

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

    std::cout << base_dir.string() << std::endl;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);

    if (!warn.empty())
        std::cout << "Warning: " << warn << std::endl;
    if (!ret) {
        std::cerr << "Failed to load GLTF file: " << err << std::endl;
        return;
    }

    std::cout << "Successfully loaded GLTF: " << filename << std::endl;
    geometries.resize(model.meshes.size());

    size_t mesh_i = 0;
    for (const auto &mesh : model.meshes) {
        size_t n_vertices = 0;
        size_t n_indices = 0;

        geometries[mesh_i].texture_index = -1;
        for (auto &primitive : mesh.primitives) {
            n_vertices += populate_vertex_data(model, primitive,
                                               geometries[mesh_i].vertices);
            n_indices += populate_index_data(model, primitive,
                                             geometries[mesh_i].indices);
            if (primitive.material != geometries[mesh_i].texture_index) {
                geometries[mesh_i].texture_index = primitive.material;
            }
        }

        std::cout << "Mesh[" << mesh_i << "]:\n";
        std::cout << "\tName: "
                  << model.materials[geometries[mesh_i].texture_index].name
                  << std::endl;
        std::cout << "\tVertices: " << n_vertices << std::endl;
        std::cout << "\tIndices: " << n_indices << std::endl;

        mesh_i++;
    }

    objects.resize(model.nodes.size());
    size_t obj_i = 0;
    for (auto &node : model.nodes) {
        if (node.mesh == -1)
            continue;
        objects[obj_i].mesh = &geometries[node.mesh];
        objects[obj_i].transformation = get_node_transform(node);
        uint32_t i = geometries[node.mesh].texture_index;

        obj_i++;
    }

    for (auto &material : model.materials) {
        materials.push_back(Material(material, model.images.data(), base_dir));

        // std::cout << material.name << std::endl;
        // Material new_material = Material(material, model.images.data(),
        // base_dir); std::cout << "Indexes" << std::endl; std::cout <<
        // material.pbrMetallicRoughness.baseColorTexture.index << std::endl;
        // std::cout <<
        // material.pbrMetallicRoughness.metallicRoughnessTexture.index <<
        // std::endl; std::cout << material.normalTexture.index << std::endl;
        // std::cout << material.emissiveTexture.index << std::endl;
        // std::cout << material.occlusionTexture.index << std::endl;

        // std::cout << "Metalic Factor: " << std::endl;;
        // std::cout << material.pbrMetallicRoughness.metallicFactor <<
        // std::endl; std::cout << material.pbrMetallicRoughness.roughnessFactor
        // << std::endl; std::cout << "Alpha cutoff: " << material.alphaCutoff
        // << std::endl; std::cout << "Alpha mode: " << material.alphaMode <<
        // std::endl; std::cout << "Base Color: ("
        //             << material.pbrMetallicRoughness.baseColorFactor[0] << ",
        //             "
        //             << material.pbrMetallicRoughness.baseColorFactor[1] << ",
        //             "
        //             << material.pbrMetallicRoughness.baseColorFactor[2] << ",
        //             "
        //             << material.pbrMetallicRoughness.baseColorFactor[3] <<
        //             ")\n";
        // for (auto& extension : material.extensions) {
        //     std::cout << extension.first << ": " <<
        //     extension.second.Get("transmissionFactor").Get<double>() <<
        //     std::endl;
        // }
        // std::cout << "Emissive Factor: \n";
        // for (auto& emi : material.emissiveFactor) {
        //     std::cout << emi << std::endl;
        // }
    }

    std::cout << "Number of meshes: " << mesh_i << std::endl;
    std::cout << "Number of objects: " << obj_i << std::endl;
    std::cout << "Number of images: " << model.images.size() << std::endl;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cout << "usage: " << argv[0] << " [path/to/model]\n";
        exit(EXIT_FAILURE);
    }

    std::string file_name(argv[1]);
    std::cout << "Loading: " << file_name << std::endl;

    Scene test_meshes(file_name);
    if (test_meshes.empty()) {
        throw std::runtime_error("Unable to load test pointer list");
    }

    // std::cout << test_meshes.size() << std::endl;
    // std::cout << test_meshes.empty() << std::endl;

    return 0;
}
