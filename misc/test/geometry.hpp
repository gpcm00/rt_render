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
#include <vector>
#include <string>
#include <cstddef>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

    bool operator==(const Vertex& other) const {
        return position == other.position;
    }
};

struct Mesh {
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
};

struct Object {
    const Mesh* mesh;
    glm::mat4 transformation;
};

class Scene {
    private:
    std::vector<Mesh> geometries;
    std::vector<Object> objects;

    public:
    Scene(const std::string& filename); 

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
};