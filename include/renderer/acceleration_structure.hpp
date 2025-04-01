#pragma once
#include <renderer/vulkan.hpp>
#include <glm/glm.hpp>
// #include <geometry/geometry.hpp>
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

    TopLevelAccelerationStructure(vk::Device & device, 
        vk::detail::DispatchLoaderDynamic & dl,
        vk::CommandPool & command_pool,
        int queue_family_index): 
         device(device), dl(dl), 
         command_pool(command_pool), 
         queue_family_index(queue_family_index) { 

            // for (auto& object : scene) {
            //     InstanceBuffer current_instance;
            //     auto it = meshes.find(object.mesh);
            //     if (it == meshes.end()) {
            //         create_mesh_buffer(object.mesh);
            //         it = meshes.find(object.mesh);
            //         if (it == meshes.end()) {
            //             throw std::runtime_error("Failed to create mesh buffer");
            //         }
        
            //         create_BLAS(&it->second);
            //     } 
            //     // create_device_buffer(current_instance.buffer, current_instance.memory, &object.transformation, sizeof(glm::mat4), 
            //     //                                                                                     vk::BufferUsageFlagBits::eVertexBuffer);
            //     current_instance.mesh_buffer = &it->second;
            //     current_instance.transformation = object.transformation;
            //     objects.push_back(current_instance);
            // }
        
            // create_TLAS();

    }

    ~TopLevelAccelerationStructure() {
        // device.destroyBuffer(buffer);
        // device.freeMemory(allocation);
    }

    private:

    // void create_BLAS(const MeshBuffer* mesh) {
    //     vk::BufferUsageFlags usage;

    //     vk::AccelerationStructureGeometryTrianglesDataKHR triangles{};
    //     triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
    //     triangles.vertexData.deviceAddress = get_device_address(mesh->vertex_buffer);
    //     triangles.vertexStride = sizeof(Vertex);
    //     triangles.indexType = vk::IndexType::eUint32;
    //     triangles.indexData.deviceAddress = get_device_address(mesh->index_buffer);
    
    //     vk::AccelerationStructureGeometryKHR geometry{};
    //     geometry.geometryType = vk::GeometryTypeKHR::eTriangles;
    //     geometry.geometry.triangles = triangles;
    //     geometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
    
    //     vk::AccelerationStructureBuildGeometryInfoKHR info{};
    //     info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    //     info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    //     info.geometryCount = 1;
    //     info.pGeometries = &geometry;
    
    //     const uint32_t primitive_count = mesh->size / 3;
    //     vk::AccelerationStructureBuildSizesInfoKHR size_info{};
    //     size_info = device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, info, primitive_count, dl);
    
    //     usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
    //     AccelerationBuffer current{};
    //     create_buffer(current.buffer, current.memory, size_info.accelerationStructureSize, usage, vk::MemoryPropertyFlagBits::eDeviceLocal);
    
    //     vk::AccelerationStructureCreateInfoKHR create_info{};
    //     create_info.buffer = current.buffer;
    //     create_info.size = size_info.accelerationStructureSize;
    //     create_info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    //     current.as = device.createAccelerationStructureKHR(create_info, nullptr, dl);
    
    //     usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
    //     AccelerationBuffer scratch{};  
    //     create_buffer(scratch.buffer, scratch.memory, size_info.accelerationStructureSize, usage, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);    
        
    //     vk::AccelerationStructureBuildGeometryInfoKHR scratch_info{};
    //     scratch_info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    //     scratch_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    //     scratch_info.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
    //     scratch_info.geometryCount = 1;
    //     scratch_info.pGeometries = &geometry;
    //     scratch_info.dstAccelerationStructure = current.as;
    //     scratch_info.scratchData.deviceAddress = get_device_address(scratch.buffer);
    
    //     vk::AccelerationStructureBuildRangeInfoKHR range_info{};
    //     range_info.primitiveCount = primitive_count;
    
    //     std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> range_infos = {&range_info};
    
    //     vk::CommandBuffer command = pool.single_command_buffer(vk::CommandBufferLevel::ePrimary);
    //     command.buildAccelerationStructuresKHR(scratch_info, range_infos, dl);
    //     command.end();
    //     pool.submit_command(command);
    //     pool.free_command_buffer(command);
    
    //     device.destroyBuffer(scratch.buffer);
    //     device.freeMemory(scratch.memory);
    
    //     vk::AccelerationStructureDeviceAddressInfoKHR address_info{};
    //     address_info.accelerationStructure = current.as;
    //     current.as_addr = device.getAccelerationStructureAddressKHR(address_info, dl);
    
    //     blas[mesh] = current;
    // }
};
