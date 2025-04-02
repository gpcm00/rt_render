#pragma once
// #ifndef STB_IMAGE_IMPLEMENTATION
// #define STB_IMAGE_IMPLEMENTATION  
// #endif

// #ifndef STB_IMAGE_WRITE_IMPLEMENTATION
// #define STB_IMAGE_WRITE_IMPLEMENTATION 
// #endif

#include <renderer/vulkan.hpp>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <cstddef>
#include <filesystem>
#include <tiny_gltf.h>


class TextureMap {
    uint8_t* map = nullptr;
    int w = 0, h = 0, c = 0;
    
    public:
    enum TextureType {
        baseColorTexture,
        normalTexture,
        emissiveTexture,
        occlusionTexture,
        metallicRoughness,
    };
    
    TextureMap() = default;
    TextureMap(std::string file_path, TextureType texture_type);

    uint8_t* data() { return map; }
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

    Material(tinygltf::Material& material, tinygltf::Model & model, std::filesystem::path base_directory) : dir(base_directory) {
        name = material.name;

        memcpy(base_color, material.pbrMetallicRoughness.baseColorFactor.data(), sizeof(base_color));
        memcpy(emissive, material.emissiveFactor.data(), sizeof(emissive));

        roughness = material.pbrMetallicRoughness.roughnessFactor;
        metallic = material.pbrMetallicRoughness.metallicFactor;
        
        const auto& it = material.extensions.find("KHR_materials_transmission");
        if (it != material.extensions.end()) {
            transmission = it->second.Get("transmissionFactor").Get<double>();
        } else {
            transmission = 0;
        }

        std::cout<< name << ": \n";
        if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
            uint32_t texture_index = material.pbrMetallicRoughness.baseColorTexture.index;
            uint32_t i = model.textures[texture_index].source;

            std::string file_path = (dir / model.images[i].uri).string();
            textures.push_back(TextureMap(file_path, TextureMap::TextureType::baseColorTexture));
            std::cout << "\tBase color: " << model.images[i].uri << std::endl;
        }

        if (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
            uint32_t texture_index = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
            uint32_t i = model.textures[texture_index].source;
            std::string file_path = (dir / model.images[i].uri).string();
            textures.push_back(TextureMap(file_path, TextureMap::TextureType::metallicRoughness));
            std::cout << "\tMetallic Roughness Texture: " << model.images[i].uri << std::endl;
        }

        if (material.normalTexture.index  >= 0) {
            uint32_t texture_index = material.normalTexture.index;
            uint32_t i = model.textures[texture_index].source;

            std::string file_path = (dir / model.images[i].uri).string();
            textures.push_back(TextureMap(file_path, TextureMap::TextureType::normalTexture));
            std::cout << "\tNormal Texture: " << model.images[i].uri << std::endl;
        }

        if (material.emissiveTexture.index  >= 0) {
            uint32_t texture_index = material.emissiveTexture.index;
            uint32_t i = model.textures[texture_index].source;

            std::string file_path = (dir / model.images[i].uri).string();
            textures.push_back(TextureMap(file_path, TextureMap::TextureType::emissiveTexture));
            std::cout << "\tEmissive Texture: " << model.images[i].uri << std::endl;
        }

        if (material.occlusionTexture.index  >= 0) {
            uint32_t texture_index = material.occlusionTexture.index;
            uint32_t i = model.textures[texture_index].source;

            std::string file_path = (dir / model.images[i].uri).string();
            textures.push_back(TextureMap(file_path, TextureMap::TextureType::occlusionTexture));
            std::cout << "\tOcclusion Texture: " << model.images[i].uri << std::endl;
        }
    }

    void cleanup() {
        for (auto& texture : textures) {
            texture.free_texture_map();
        }
    }

    auto begin() {
        return textures.begin();
    }

    auto end() {
        return textures.end();
    }
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uvmap;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

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

    bool operator==(const Vertex& other) const {
        return position == other.position;
    }
};

struct Mesh {
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    int texture_index;
    uint32_t mesh_id;
};

struct Object {
    const Mesh* mesh;
    glm::mat4 transformation;
    glm::mat4 global_transformation;
};

class Scene {
    private:
    std::vector<Mesh> geometries;
    std::vector<Object> objects;
    std::vector<Material> materials;


    public:
    Scene(const std::string& filename); 
    ~Scene() { for (auto material : materials) material.cleanup(); }

    bool empty() {
        return geometries.empty() || objects.empty();
    }

    size_t size() {
        return objects.size();
    }

    Mesh mesh(size_t i) {
        return (!geometries.empty() && i < geometries.size()) ? geometries[i] : Mesh();
    }

    Object node(size_t i) {
        return (!objects.empty() && i < objects.size()) ? objects[i] : Object();
    }

    auto begin() { 
        return objects.begin(); 
    }

    auto end() {
        return objects.end();
    }

    std::vector<Mesh>& get_geometries() {
        return geometries;
    }
};