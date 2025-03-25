#include <renderer/renderer.hpp>

static VkTransformMatrixKHR from_mat4(glm::mat4& mat) {
    VkTransformMatrixKHR ret{};
    for (int r = 0 ; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            ret.matrix[r][c] = mat[r][c];
        }
    }
    return ret;
}

static VkWriteDescriptorSet populate_write_descriptor(VkBuffer buffer, VkDeviceSize size, VkDescriptorSet set, VkDescriptorType type, uint32_t binding) {
    VkDescriptorBufferInfo bi{};
    bi.buffer = buffer;
    bi.range = size;
    
    VkWriteDescriptorSet wds{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wds.dstSet = set;
    wds.descriptorCount = 1;
    wds.dstBinding = binding;
    wds.pBufferInfo = &bi;
    wds.descriptorType = type;

    return wds;
}

static void submit_copy_command(Command_Pool& pool, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBuffer copy_command = pool.single_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(copy_command, &info);

    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(copy_command, src, dst, 1, &region);

    vkEndCommandBuffer(copy_command);

    pool.submit_command(copy_command);
    pool.free_command_buffer(copy_command);
}

void Renderer::create_descriptor_sets() {
    std::vector<VkDescriptorPoolSize> pool_sizes {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
        // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
    };

    VkDescriptorPool pool;

    VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = 1;
    vkCreateDescriptorPool((VkDevice)device, &pool_info, nullptr, &pool);

    VkDescriptorSet descriptor_set;
    VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc_info.descriptorPool = pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = pipeline.get_set();

    VkWriteDescriptorSetAccelerationStructureKHR dsas_info{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    dsas_info.accelerationStructureCount = 1;
    dsas_info.pAccelerationStructures = &tlas.as.as;

    VkWriteDescriptorSet accel_wds{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; 
    accel_wds.descriptorCount = 1;
    accel_wds.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    accel_wds.dstBinding = 0;
    accel_wds.dstSet = descriptor_set;
    accel_wds.pNext = &dsas_info;

    // VkWriteDescriptorSet image_wds = populate_write_descriptor()
    VkWriteDescriptorSet camera_wds = populate_write_descriptor(camera.buffer, sizeof(camera.matrices), descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);

    std::vector<VkWriteDescriptorSet> wds = {
        accel_wds,
        camera_wds,
    };

    vkUpdateDescriptorSets((VkDevice)device, static_cast<uint32_t>(wds.size()), wds.data(), 0, VK_NULL_HANDLE);
}

void Renderer::create_ubo() {
    create_buffer(camera.map.buffer, camera.map.memory, sizeof(Camera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkMapMemory((VkDevice)device, camera.map.memory, 0, sizeof(Camera), 0, &camera.map.data);
}

void Renderer::create_device_buffer(VkBuffer& buffer, VkDeviceMemory& memory, const void* host_buffer, 
                                                            VkDeviceSize size, VkBufferUsageFlags usage) {
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    create_buffer(staging_buffer, staging_buffer_memory, size, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(device, staging_buffer_memory, 0, size, 0, &data);
    memcpy(data, host_buffer, (size_t)size);
    vkUnmapMemory(device, staging_buffer_memory);

    create_buffer(buffer, memory, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    submit_copy_command(pool, staging_buffer, buffer, size);

    vkDestroyBuffer(device, staging_buffer, nullptr);
    vkFreeMemory(device, staging_buffer_memory, nullptr);
}

void Renderer::create_buffer(VkBuffer& buffer, VkDeviceMemory& memory, VkDeviceSize size, VkBufferUsageFlags usage, 
                                        VkMemoryPropertyFlags properties, VkSharingMode mode = VK_SHARING_MODE_EXCLUSIVE) {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = mode;

    if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vkBuffer");
    }

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, buffer, &requirements);

    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = requirements.size;

    uint32_t mem_idx = 0;
    for (; mem_idx < mem_properties.memoryTypeCount; mem_idx++) {
        if ((requirements.memoryTypeBits & (1 << mem_idx)) 
        && ((mem_properties.memoryTypes[mem_idx].propertyFlags & properties) == properties)) {
            alloc_info.memoryTypeIndex = mem_idx;
            break;
        }
    }

    if (mem_idx == mem_properties.memoryTypeCount) {
        throw std::runtime_error("No suitable memory type in device");
    }

    if (vkAllocateMemory(device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate VkDeviceMemory");
    }

    vkBindBufferMemory(device, buffer, memory, 0);
}

void Renderer::create_mesh_buffer(const Mesh* mesh) {
    MeshBuffer mesh_buffer;

    VkBufferUsageFlags flags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | 
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    const void* data = mesh->vertices.data();
    VkDeviceSize size = mesh->vertices.size() * sizeof(Vertex);
    create_device_buffer(mesh_buffer.vertex_buffer, mesh_buffer.vertex_memory, data, size, flags);

    data =  mesh->indices.data();
    size = mesh->indices.size() * sizeof(uint32_t);
    create_device_buffer(mesh_buffer.vertex_buffer, mesh_buffer.vertex_memory, data, size, flags);

    mesh_buffer.size = size;
    meshes[mesh] = mesh_buffer;
}

void Renderer::create_BLAS(const MeshBuffer* mesh) {
    VkBufferUsageFlags usage = 0;

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = get_device_address(mesh->vertex_buffer);
    triangles.vertexStride = sizeof(Vertex);
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = get_device_address(mesh->index_buffer);

    VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles = triangles;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    info.geometryCount = 1;
    info.pGeometries = &geometry;

    const uint32_t primitive_count = mesh->size / 3;
    VkAccelerationStructureBuildSizesInfoKHR size_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR((VkDevice)device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, &primitive_count, &size_info);

    usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    AccelerationBuffer current{};
    create_buffer(current.buffer, current.memory, size_info.accelerationStructureSize, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkAccelerationStructureCreateInfoKHR create_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    create_info.buffer = current.buffer;
    create_info.size = size_info.accelerationStructureSize;
    create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    vkCreateAccelerationStructureKHR((VkDevice)device, &create_info, nullptr, &current.as);

    usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    AccelerationBuffer scratch{};  
    create_buffer(scratch.buffer, scratch.memory, size_info.accelerationStructureSize, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);    
    
    VkAccelerationStructureBuildGeometryInfoKHR scratch_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    scratch_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    scratch_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    scratch_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    scratch_info.geometryCount = 1;
    scratch_info.pGeometries = &geometry;
    scratch_info.dstAccelerationStructure = current.as;
    scratch_info.scratchData.deviceAddress = get_device_address(scratch.buffer);

    VkAccelerationStructureBuildRangeInfoKHR range_info{};
    range_info.primitiveCount = primitive_count;

    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> range_infos = {&range_info};

    VkCommandBuffer command = pool.single_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    vkCmdBuildAccelerationStructuresKHR(command, 1, &scratch_info, range_infos.data());
    vkEndCommandBuffer(command);
    pool.submit_command(command);
    pool.free_command_buffer(command);

    vkDestroyBuffer((VkDevice)device, scratch.buffer, nullptr);
    vkFreeMemory((VkDevice)device, scratch.memory, nullptr);

    VkAccelerationStructureDeviceAddressInfoKHR address_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    address_info.accelerationStructure = current.as;
    current.as_addr = vkGetAccelerationStructureDeviceAddressKHR((VkDevice)device, &address_info);

    blas[mesh] = current;
}

void Renderer::create_TLAS() {
    VkBufferUsageFlags usage = 0;
    
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    uint32_t instance_index = 0;
    for (auto& object : objects) {
        VkTransformMatrixKHR transform = from_mat4(object.transformation);
        AccelerationBuffer current_blas = blas[object.mesh_buffer];

        VkAccelerationStructureInstanceKHR instance{};
        instance.transform = transform;
        instance.instanceCustomIndex = instance_index++;
        instance.mask = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR; // maybe use VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR
        instance.accelerationStructureReference = current_blas.as_addr;

        instances.push_back(instance);
    }

    usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VkDeviceSize size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
    create_device_buffer(tlas.buffer, tlas.memory, instances.data(), size, usage);

    VkDeviceOrHostAddressConstKHR instance_address{};
    instance_address.deviceAddress = get_device_address(tlas.buffer);

    VkAccelerationStructureGeometryInstancesDataKHR geometry_data{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    geometry_data.arrayOfPointers = VK_FALSE;
    geometry_data.data = instance_address;

    VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.instances = geometry_data;

    VkAccelerationStructureBuildGeometryInfoKHR geometry_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    geometry_info.geometryCount = 1;
    geometry_info.pGeometries = &geometry;

    const uint32_t primitive_count = instances.size();

    VkAccelerationStructureBuildSizesInfoKHR size_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR((VkDevice)device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &geometry_info, &primitive_count, &size_info);
    usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    create_buffer(tlas.as.buffer, tlas.as.memory, size_info.accelerationStructureSize, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkAccelerationStructureCreateInfoKHR create_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    create_info.buffer = tlas.buffer;
    create_info.size = size_info.accelerationStructureSize;
    create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    vkCreateAccelerationStructureKHR((VkDevice)device, &create_info, nullptr, &tlas.as.as);

    usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    AccelerationBuffer scratch{};  
    create_buffer(scratch.buffer, scratch.memory, size_info.accelerationStructureSize, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkAccelerationStructureBuildGeometryInfoKHR scratch_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    scratch_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    scratch_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;  //  note: add VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR for dynamic scenes
    scratch_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    scratch_info.geometryCount = 1;
    scratch_info.pGeometries = &geometry;
    scratch_info.dstAccelerationStructure = tlas.as.as;
    scratch_info.scratchData.deviceAddress = get_device_address(scratch.buffer);

    VkAccelerationStructureBuildRangeInfoKHR range_info{};
    range_info.primitiveCount = primitive_count;

    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> range_infos = {&range_info};
    VkCommandBuffer command = pool.single_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    vkCmdBuildAccelerationStructuresKHR(command, 1, &scratch_info, range_infos.data());
    vkEndCommandBuffer(command);
    pool.submit_command(command);
    pool.free_command_buffer(command);

    // note: keep scratch for dynamic scenes to avoid allocation overhead
    vkDestroyBuffer((VkDevice)device, scratch.buffer, nullptr);
    vkFreeMemory((VkDevice)device, scratch.memory, nullptr);

    VkAccelerationStructureDeviceAddressInfoKHR address_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    address_info.accelerationStructure = tlas.as.as;
    tlas.as.as_addr = vkGetAccelerationStructureDeviceAddressKHR((VkDevice)device, &address_info);
}

void Renderer::load_scene(std::string file_path) {
    scene = Scene(file_path);

    for (auto& object : scene) {
        InstanceBuffer current_instance;
        auto it = meshes.find(object.mesh);
        if (it == meshes.end()) {
            create_mesh_buffer(object.mesh);
            it = meshes.find(object.mesh);
            if (it == meshes.end()) {
                throw std::runtime_error("Failed to create mesh buffer");
            }

            create_BLAS(&it->second);
        } 
        // create_device_buffer(current_instance.buffer, current_instance.memory, &object.transformation, sizeof(glm::mat4), 
        //                                                                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        current_instance.mesh_buffer = &it->second;
        current_instance.transformation = object.transformation;
        objects.push_back(current_instance);
    }

    create_TLAS();
}

void Renderer::create_sbt() {
    VkPhysicalDeviceProperties2 properties;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_properties;

    rt_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &rt_properties;
    vkGetPhysicalDeviceProperties2((VkPhysicalDevice)physical_device, &properties);

    const uint32_t handle_size = rt_properties.shaderGroupHandleSize;
    const uint32_t handle_alignment = rt_properties.shaderGroupHandleAlignment;
    const uint32_t handle_size_aligned = (handle_size+handle_alignment-1)&~(handle_alignment-1);
    const uint32_t count = pipeline.shader_count();
    const uint32_t sbt_size = count * handle_size_aligned;
    const VkBufferUsageFlags usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    uint8_t* handle_storage;
    vkGetRayTracingShaderGroupHandlesKHR((VkDevice)device, pipeline.pipeline_data(), 0, count, sbt_size, handle_storage);

    // SBTs.resize(count);
    // uint32_t chunk = 0;
    // for (auto& SBT : SBTs) {
    //     create_device_buffer(SBT.buffer, SBT.memory, handle_storage + chunk * handle_size_aligned, handle_size, usage);
    // }

    create_device_buffer(rgen_sbt.buffer, rgen_sbt.memory, handle_storage, handle_size, usage);
    create_device_buffer(miss_sbt.buffer, miss_sbt.memory, handle_storage + handle_size_aligned, handle_size, usage);
    create_device_buffer(hit_sbt.buffer, hit_sbt.memory, handle_storage + handle_size_aligned*2, handle_size, usage);
}