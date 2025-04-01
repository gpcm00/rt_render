#pragma once
#include <renderer/vulkan.hpp>
#include <glm/glm.hpp>
#include <unordered_map>

struct StagingBuffer {
    void* data;
    
    vk::Buffer buffer;
    vk::DeviceMemory memory;
};


struct UnifromBuffer {
    Camera matrices;

    vk::Buffer buffer;
    vk::DeviceMemory memory;

    StagingBuffer map;
};

struct MeshBuffer {
    vk::Buffer vertex_buffer;
    VmaAllocation vertex_allocation;

    vk::Buffer index_buffer;
    VmaAllocation index_allocation;
    
    uint32_t num_indices;
    // uint32_t index_buffer_size;
};

// struct InstanceBuffer {
//     MeshBuffer* mesh_buffer;
//     glm::mat4 transformation;
// };

class InstanceBuffer {
    public:
    MeshBuffer* mesh_buffer;
    glm::mat4 transformation;

    InstanceBuffer(MeshBuffer* mesh_buffer, glm::mat4 & transformation): 
        mesh_buffer(mesh_buffer), transformation(transformation) { }
};


struct AccelerationBuffer {
    vk::AccelerationStructureKHR as;

    vk::Buffer buffer;
    VmaAllocation allocation;
    // vk::DeviceMemory memory;
    vk::DeviceAddress as_addr;
};

/*
struct TopAccelerationBuffer {
    AccelerationBuffer as;

    // needs a separated buffer for instances
    vk::Buffer buffer;
    vk::DeviceMemory memory;
};
*/

struct MeshData {
    vk::DeviceAddress vertex;
    vk::DeviceAddress index;
};

struct InstanceData {
    uint32_t mesh_id;
};

class TopLevelAccelerationStructure {
    private:
    // Need these for freeing resources
    vk::Device & device;
    vk::detail::DispatchLoaderDynamic & dl;
    int queue_family_index;
    vk::CommandPool & command_pool;
    public:
    // Populate these externally
    vk::AccelerationStructureKHR structure;
    vk::Buffer buffer;
    VmaAllocation allocation;
    vk::DeviceAddress addr;

    // std::vector<vk::AccelerationStructureInstanceKHR> instances;
    std::vector<InstanceBuffer> instance_buffers;

    std::unordered_map<const Mesh*, MeshBuffer> meshes;

    std::unordered_map<const MeshBuffer*, AccelerationBuffer> blas;

    // This is a lookup table for vertex and index buffers
    std::vector<MeshData> mesh_data;
    vk::Buffer mesh_data_buffer;
    VmaAllocation mesh_data_allocation;

    // This stores data associated with each instance
    // for in-shader acccess
    std::vector<InstanceData> instance_data;
    vk::Buffer instance_data_buffer;
    VmaAllocation instance_data_allocation;

    TopLevelAccelerationStructure(vk::Device & device, 
        vk::detail::DispatchLoaderDynamic & dl,
        vk::CommandPool & command_pool,
        int queue_family_index): 
         device(device), dl(dl), 
         command_pool(command_pool), 
         queue_family_index(queue_family_index) { 

    }
    
};
