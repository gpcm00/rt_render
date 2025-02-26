#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 texCoord;

    bool operator==(const Vertex& other) const {
        return position == other.position;
    }
};

struct Mesh {
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
};

struct Object {
    const Mesh *mesh;
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
};