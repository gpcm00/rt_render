#pragma once
// #ifndef STB_IMAGE_IMPLEMENTATION
// #define STB_IMAGE_IMPLEMENTATION  
// #endif

// #ifndef STB_IMAGE_WRITE_IMPLEMENTATION
// #define STB_IMAGE_WRITE_IMPLEMENTATION 
// #endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#define STB_IMAGE_IMPLEMENTATION  
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #include <stb_image.h>
#include <vector>
#include <string>
#include <cstddef>
#include <filesystem>


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
    TextureMap(std::string file_path, TextureType texture_type) : type(texture_type) {
        map = stbi_load(file_path.c_str(), &w, &h, &c, STBI_rgb_alpha);
    }

    uint8_t* data() { return map; }
    int height() { return h; } 
    int width() { return w; } 
    int channels() { return c; } 

    void free_texture_map() {
        stbi_image_free(map);
    }

    private:
    TextureType type;
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

    Material(tinygltf::Material& material, tinygltf::Image* images, std::filesystem::path base_directory) : dir(base_directory) {
        name = material.name;

        memcpy(base_color, material.pbrMetallicRoughness.baseColorFactor.data(), 4*sizeof(double));
        memcpy(emissive, material.emissiveFactor.data(), 3*sizeof(double));

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
            uint32_t i = material.pbrMetallicRoughness.baseColorTexture.index;
            std::string file_path = (dir / images[i].uri).string();
            textures.push_back(TextureMap(file_path, TextureMap::TextureType::baseColorTexture));
            std::cout << "\tBase color: " << images[i].uri << std::endl;
        } else {
            std::cout << "\tNo base color\n";
        }

        if (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
            uint32_t i = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
            std::string file_path = (dir / images[i].uri).string();
            textures.push_back(TextureMap(file_path, TextureMap::TextureType::metallicRoughness));
            std::cout << "\tMetallic Roughness Texture: " << images[i].uri << std::endl;
        } else {
            std::cout << "\tNo Metallic Roughness Texture\n";
        }

        if (material.normalTexture.index  >= 0) {
            uint32_t i = material.normalTexture.index;
            std::string file_path = (dir / images[i].uri).string();
            textures.push_back(TextureMap(file_path, TextureMap::TextureType::normalTexture));
            std::cout << "\tNormal Texture: " << images[i].uri << std::endl;
        } else {
            std::cout << "\tNo Normal Texture\n";
        }

        if (material.emissiveTexture.index  >= 0) {
            uint32_t i = material.emissiveTexture.index;
            std::string file_path = (dir / images[i].uri).string();
            textures.push_back(TextureMap(file_path, TextureMap::TextureType::emissiveTexture));
            std::cout << "\tEmissive Texture: " << images[i].uri << std::endl;
        } else {
            std::cout << "\tNo Emissive Texture\n";
        }

        if (material.occlusionTexture.index  >= 0) {
            uint32_t i = material.occlusionTexture.index;
            std::string file_path = (dir / images[i].uri).string();
            textures.push_back(TextureMap(file_path, TextureMap::TextureType::occlusionTexture));
            std::cout << "\tOcclusion Texture: " << images[i].uri << std::endl;
        } else {
            std::cout << "\tNo Occlusion Texture\n";
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

    uint32_t texture_index;
};

struct Object {
    const Mesh* mesh;
    glm::mat4 transformation;
};

class Scene {
    private:
    std::vector<Mesh> geometries;
    std::vector<Object> objects;
    std::vector<Material> materials;

    public:
    Scene() = default;
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

    Material material(size_t i) {
        return (!materials.empty() && i < materials.size()) ? materials[i] : Material();
    }

    auto begin() { 
        return objects.begin(); 
    }

    auto end() {
        return objects.end();
    }
};