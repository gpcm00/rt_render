#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION  
#define STB_IMAGE_WRITE_IMPLEMENTATION  
#include <iostream>
#include <cstdlib>
#include <vector>
#include <tiny_gltf.h>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

void LoadGLTFModel(const std::string& filename) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename); 

    if (!warn.empty()) std::cout << "Warning: " << warn << std::endl;
    if (!ret) {
        std::cerr << "Failed to load GLTF file: " << err << std::endl;
        return;
    }

    std::cout << "Successfully loaded GLTF: " << filename << std::endl;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
            const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];

            const float* posData = reinterpret_cast<const float*>(&posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);

            for (size_t i = 0; i < posAccessor.count; i++) {
                Vertex vertex;
                vertex.position = glm::vec3(posData[i * 3], posData[i * 3 + 1], posData[i * 3 + 2]);

                if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                    const tinygltf::Accessor& normalAccessor = model.accessors[primitive.attributes.at("NORMAL")];
                    const tinygltf::BufferView& normalView = model.bufferViews[normalAccessor.bufferView];
                    const tinygltf::Buffer& normalBuffer = model.buffers[normalView.buffer];

                    const float* normalData = reinterpret_cast<const float*>(&normalBuffer.data[normalView.byteOffset + normalAccessor.byteOffset]);
                    vertex.normal = glm::vec3(normalData[i * 3], normalData[i * 3 + 1], normalData[i * 3 + 2]);
                } else {
                    vertex.normal = glm::vec3(0.0f); // Default normal
                }

                vertices.push_back(vertex);
            }

            if (primitive.indices >= 0) {
                const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];

                const void* indexData = &indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset];

                for (size_t i = 0; i < indexAccessor.count; i++) {
                    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        indices.push_back(reinterpret_cast<const uint16_t*>(indexData)[i]);
                    } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        indices.push_back(reinterpret_cast<const uint32_t*>(indexData)[i]);
                    }
                }
            }
        }
    }

    std::cout << "Loaded Mesh: " << vertices.size() << " vertices, " << indices.size() << " indices" << std::endl;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "usage: " << argv[0] << "[path/to/model]\n";
        exit(EXIT_FAILURE);
    }
    
    std::string file_name(argv[1]);

    std::cout << "Loading: " << file_name << std::endl;
    LoadGLTFModel(file_name);
    return 0;
}
