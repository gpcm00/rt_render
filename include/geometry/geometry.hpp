#pragma once

#include <renderer/vulkan.hpp>

#include <GLFW/glfw3.h>
#include <cstddef>
#include <filesystem>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <tiny_gltf.h>
#include <vector>

class TextureMap {
    std::vector<unsigned char> map;
    int w = 0, h = 0, c = 0;

  public:
    enum TextureType {
        baseColorTexture,
        normalTexture,
        emissiveTexture,
        occlusionTexture,
        metallicRoughnessTexture,
    };

    TextureMap() = default;
    TextureMap(tinygltf::Image &image, TextureType texture_type);
    TextureMap(glm::vec4 value, TextureType texture_type);

    uint8_t *data() { return map.data(); }
    int height() { return h; }
    int width() { return w; }
    int channels() { return c; }

    void free_texture_map();

    TextureType type() { return texture_type; }

  private:
    TextureType texture_type;
};

class Material {
    std::string name;
    std::filesystem::path dir;

    double base_color[4];
    double emissive[3];
    double metallic;
    double roughness;
    double transmission;

    std::vector<TextureMap> textures;

  public:
    Material() = default;

    Material(tinygltf::Material &material, tinygltf::Model &model,
             std::filesystem::path base_directory)
        : dir(base_directory) {
        name = material.name;

        memcpy(base_color, material.pbrMetallicRoughness.baseColorFactor.data(),
               sizeof(base_color));
        memcpy(emissive, material.emissiveFactor.data(), sizeof(emissive));

        roughness = material.pbrMetallicRoughness.roughnessFactor;
        metallic = material.pbrMetallicRoughness.metallicFactor;

        const auto &it = material.extensions.find("KHR_materials_transmission");
        if (it != material.extensions.end()) {
            transmission = it->second.Get("transmissionFactor").Get<double>();
        } else {
            transmission = 0;
        }

        std::cout << name << ": \n";
        if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
            uint32_t texture_index =
                material.pbrMetallicRoughness.baseColorTexture.index;
            uint32_t i = model.textures[texture_index].source;
            std::string file_path = (dir / model.images[i].uri).string();
            if (model.images[i].component != 4) {
                throw std::runtime_error(
                    "Base color texture must have 4 channels");
            }
            textures.push_back(TextureMap(
                model.images[i], TextureMap::TextureType::baseColorTexture));
            std::cout << "\tBase color: " << model.images[i].uri << std::endl;
        } else {
            // Create a 1x1 texture with the base color
            glm::vec4 base_color_vec(base_color[0], base_color[1],
                                     base_color[2], base_color[3]);
            textures.push_back(TextureMap(
                base_color_vec, TextureMap::TextureType::baseColorTexture));
        }

        if (material.normalTexture.index >= 0) {
            uint32_t texture_index = material.normalTexture.index;
            uint32_t i = model.textures[texture_index].source;

            std::string file_path = (dir / model.images[i].uri).string();
            if (model.images[i].component != 4) {
                throw std::runtime_error("Normal texture must have 4 channels");
            }
            textures.push_back(TextureMap(
                model.images[i], TextureMap::TextureType::normalTexture));
            std::cout << "\tNormal Texture: " << model.images[i].uri
                      << std::endl;
        } else {
            glm::vec4 normal_map_vec(0.0f, 0.0f, 1.0f, 0.0f);
            textures.push_back(TextureMap(
                normal_map_vec, TextureMap::TextureType::normalTexture));
        }

        if (material.emissiveTexture.index >= 0) {
            uint32_t texture_index = material.emissiveTexture.index;
            uint32_t i = model.textures[texture_index].source;

            std::string file_path = (dir / model.images[i].uri).string();

            if (model.images[i].component == 4 ||
                model.images[i].component == 3) {
                textures.push_back(TextureMap(
                    model.images[i], TextureMap::TextureType::emissiveTexture));
            }
            else {
                std::cout << "Component: " << model.images[i].component
                          << std::endl;
                throw std::runtime_error(
                    "Emissive texture must have 3 or 4 channels");
            }

            std::cout << "\tEmissive Texture: " << model.images[i].uri
                      << std::endl;
        } else {
            glm::vec4 emissive_vec(emissive[0], emissive[1], emissive[2], 0.0f);
            textures.push_back(TextureMap(
                emissive_vec, TextureMap::TextureType::emissiveTexture));
        }

        if (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
            uint32_t texture_index =
                material.pbrMetallicRoughness.metallicRoughnessTexture.index;
            uint32_t i = model.textures[texture_index].source;
            std::string file_path = (dir / model.images[i].uri).string();

            textures.push_back(
                TextureMap(model.images[i],
                           TextureMap::TextureType::metallicRoughnessTexture));
            std::cout << "\tMetallic Roughness Texture: " << model.images[i].uri
                      << std::endl;
        } else {
            glm::vec4 metallic_roughness_vec(metallic, roughness, 0.0f, 0.0f);
            textures.push_back(
                TextureMap(metallic_roughness_vec,
                           TextureMap::TextureType::metallicRoughnessTexture));
        }
    }

    void cleanup() {
        for (auto &texture : textures) {
            texture.free_texture_map();
        }
    }

    double get_transmission() { return transmission; }

    auto begin() { return textures.begin(); }

    auto end() { return textures.end(); }
};

struct Vertex {
    glm::vec3 position;
    float padding0;
    glm::vec3 normal;
    float padding1;
    glm::vec3 color;
    float padding2;
    glm::vec2 uvmap;
    glm::vec2 padding3;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 4>
    getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 4>
            attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, color);

        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, uvmap);

        return attributeDescriptions;
    }

    bool operator==(const Vertex &other) const {
        return position == other.position;
    }
};

class Primitive {
  public:
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    int32_t material_index;
    uint32_t primitive_id;
};

class Mesh {
  public:
    std::vector<Primitive> primitives;
    uint32_t mesh_id;
};

struct Object {
    const Mesh *mesh;
    glm::mat4 transformation;
    glm::mat4 global_transformation;
};

class Scene {
  private:
    std::vector<Mesh> geometries;
    std::vector<Object> objects;
    std::vector<Material> materials;

    uint32_t primitive_id;

  public:
    Scene(const std::string &filename);
    ~Scene() {
        for (auto material : materials)
            material.cleanup();
    }

    bool empty() { return geometries.empty() || objects.empty(); }

    size_t size() { return objects.size(); }

    Mesh mesh(size_t i) {
        return (!geometries.empty() && i < geometries.size()) ? geometries[i]
                                                              : Mesh();
    }

    Object node(size_t i) {
        return (!objects.empty() && i < objects.size()) ? objects[i] : Object();
    }

    auto begin() { return objects.begin(); }

    auto end() { return objects.end(); }

    Material material(size_t i) {
        return (!materials.empty() && i < materials.size()) ? materials[i]
                                                            : Material();
    }

    size_t material_size() { return materials.size(); }

    std::vector<Material> &get_materials() { return materials; }

    uint32_t num_primitives() { return primitive_id; }
};
