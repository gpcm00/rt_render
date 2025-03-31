#include <renderer/renderer.hpp>

static vk::TransformMatrixKHR from_mat4(glm::mat4& mat) {
    vk::TransformMatrixKHR ret{};
    for (int r = 0 ; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            ret.matrix[r][c] = mat[r][c];
        }
    }
    return ret;
}

static vk::WriteDescriptorSet populate_write_descriptor(vk::Buffer buffer, vk::DeviceSize size, vk::DescriptorSet set, vk::DescriptorType type, uint32_t binding) {
    vk::DescriptorBufferInfo bi{};
    bi.buffer = buffer;
    bi.range = size;
    
    vk::WriteDescriptorSet wds{};
    wds.dstSet = set;
    wds.descriptorCount = 1;
    wds.dstBinding = binding;
    wds.pBufferInfo = &bi;
    wds.descriptorType = type;

    return wds;
}

static void submit_copy_command(Command_Pool& pool, vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) {
    vk::CommandBuffer copy_command = pool.single_command_buffer(vk::CommandBufferLevel::ePrimary);

    vk::CommandBufferBeginInfo info{};
    info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    copy_command.begin(info);

    vk::BufferCopy region{};
    region.size = size;
    copy_command.copyBuffer(src, dst, region);

    copy_command.end();

    pool.submit_command(copy_command);
    pool.free_command_buffer(copy_command);
}
/*
void Renderer::create_descriptor_sets() {
    std::vector<vk::DescriptorPoolSize> pool_sizes {
        {vk::DescriptorType::eAccelerationStructureKHR, 1},
        // {vk::DescriptorType::eStorageImage, 1},
        {vk::DescriptorType::eUniformBuffer, 1},
    };

    vk::DescriptorPool pool;

    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = 1;
    pool = device.createDescriptorPool(pool_info);

    vk::DescriptorSet descriptor_set;
    vk::DescriptorSetAllocateInfo alloc_info{};
    alloc_info.descriptorPool = pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = pipeline.get_set();

    vk::WriteDescriptorSetAccelerationStructureKHR dsas_info{};
    dsas_info.accelerationStructureCount = 1;
    dsas_info.pAccelerationStructures = &tlas.as.as;

    vk::WriteDescriptorSet accel_wds{}; 
    accel_wds.descriptorCount = 1;
    accel_wds.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
    accel_wds.dstBinding = 0;
    accel_wds.dstSet = descriptor_set;
    accel_wds.pNext = &dsas_info;

    // vk::WriteDescriptorSet image_wds = populate_write_descriptor()
    vk::WriteDescriptorSet camera_wds = populate_write_descriptor(camera.buffer, sizeof(camera.matrices), descriptor_set, vk::DescriptorType::eUniformBuffer, 1);

    std::vector<vk::WriteDescriptorSet> wds = {
        accel_wds,
        camera_wds,
    };

    // device.updateDescriptorSets(wds, nullptr);
}
*/
/*
void Renderer::create_ubo() {
    create_buffer(camera.map.buffer, camera.map.memory, sizeof(Camera), vk::BufferUsageFlagBits::eUniformBuffer, 
    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    camera.map.data = device.mapMemory(camera.map.memory, 0, sizeof(Camera));
}
*/

void Renderer::create_device_buffer(vk::Buffer& buffer, vk::DeviceMemory& memory, const void* host_buffer, 
                                                            vk::DeviceSize size, vk::BufferUsageFlags usage) {
    vk::Buffer staging_buffer;
    vk::DeviceMemory staging_buffer_memory;
    create_buffer(staging_buffer, staging_buffer_memory, size, 
        vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data;
    data = device.mapMemory(staging_buffer_memory, 0, size);
    memcpy(data, host_buffer, (size_t)size);
    device.unmapMemory(staging_buffer_memory);

    create_buffer(buffer, memory, size, vk::BufferUsageFlagBits::eTransferDst | usage, vk::MemoryPropertyFlagBits::eDeviceLocal);
    
    submit_copy_command(pool, staging_buffer, buffer, size);

    device.destroyBuffer(staging_buffer);
    device.freeMemory(staging_buffer_memory);
}

void Renderer::create_buffer(vk::Buffer& buffer, vk::DeviceMemory& memory, vk::DeviceSize size, vk::BufferUsageFlags usage, 
                                        vk::MemoryPropertyFlags properties, vk::SharingMode mode) {
    vk::BufferCreateInfo buffer_info{};
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = mode;

    buffer = device.createBuffer(buffer_info);

    vk::MemoryRequirements requirements{};
    requirements = device.getBufferMemoryRequirements(buffer);

    vk::PhysicalDeviceMemoryProperties mem_properties;
    physical_device.getMemoryProperties(&mem_properties);

    vk::MemoryAllocateInfo alloc_info{};
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

    memory = device.allocateMemory(alloc_info);

    device.bindBufferMemory(buffer, memory, 0);
}

void Renderer::create_mesh_buffer(TopLevelAccelerationStructure* tlas, const Mesh* mesh) {
    MeshBuffer mesh_buffer;

    vk::BufferUsageFlags flags = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | 
                               vk::BufferUsageFlagBits::eShaderDeviceAddress;

    // Vertex buffer
    // const void* data = mesh->vertices.data();
    // vk::DeviceSize size = mesh->vertices.size() * sizeof(Vertex);
    // create_device_buffer(mesh_buffer.vertex_buffer, mesh_buffer.vertex_memory, data, size, flags);
    auto [vertex_buffer, vertex_allocation] 
        = create_device_buffer_with_data(mesh->vertices.data(), mesh->vertices.size() * sizeof(Vertex), flags);
    mesh_buffer.vertex_buffer = vertex_buffer;
    mesh_buffer.vertex_allocation = vertex_allocation;
    // Index buffer
    // data =  mesh->indices.data();
    // size = mesh->indices.size() * sizeof(uint32_t);
    auto [index_buffer, index_allocation] = 
        create_device_buffer_with_data(mesh->indices.data(), mesh->indices.size() * sizeof(uint32_t), flags);
    mesh_buffer.index_buffer = index_buffer;
    mesh_buffer.index_allocation = index_allocation;
    // create_device_buffer(mesh_buffer.vertex_buffer, mesh_buffer.vertex_memory, data, size, flags);
    // mesh_buffer.size = size;

    mesh_buffer.size = mesh->indices.size() * sizeof(uint32_t);
    tlas->meshes[mesh] = mesh_buffer;
}

void Renderer::create_BLAS(TopLevelAccelerationStructure * tlas, const MeshBuffer* mesh) {
    vk::BufferUsageFlags usage;

    vk::AccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
    triangles.vertexData.deviceAddress = get_device_address(mesh->vertex_buffer);
    triangles.vertexStride = sizeof(Vertex);
    triangles.indexType = vk::IndexType::eUint32;
    triangles.indexData.deviceAddress = get_device_address(mesh->index_buffer);

    vk::AccelerationStructureGeometryKHR geometry{};
    geometry.geometryType = vk::GeometryTypeKHR::eTriangles;
    geometry.geometry.triangles = triangles;
    geometry.flags = vk::GeometryFlagBitsKHR::eOpaque;

    vk::AccelerationStructureBuildGeometryInfoKHR info{};
    info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    info.geometryCount = 1;
    info.pGeometries = &geometry;

    const uint32_t primitive_count = mesh->size / 3;
    vk::AccelerationStructureBuildSizesInfoKHR size_info{};
    size_info = device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, info, primitive_count, dl);

    usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
    AccelerationBuffer current{};
    // create_buffer(current.buffer, current.memory, size_info.accelerationStructureSize, usage, vk::MemoryPropertyFlagBits::eDeviceLocal);
    auto [current_buffer, current_allocation] = 
        create_device_buffer(size_info.accelerationStructureSize, usage);
    current.buffer = current_buffer;
    current.allocation = current_allocation;
    
    vk::AccelerationStructureCreateInfoKHR create_info{};
    create_info.buffer = current_buffer;
    create_info.size = size_info.accelerationStructureSize;
    create_info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    current.as = device.createAccelerationStructureKHR(create_info, nullptr, dl);
    usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
    // AccelerationBuffer scratch{};  
    // create_buffer(scratch.buffer, scratch.memory, size_info.accelerationStructureSize, usage, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);    
    auto [scratch_buffer, scratch_allocation] = 
        create_device_buffer(size_info.buildScratchSize, usage);

    vk::AccelerationStructureBuildGeometryInfoKHR scratch_info{};
    scratch_info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    scratch_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    scratch_info.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
    scratch_info.geometryCount = 1;
    scratch_info.pGeometries = &geometry;
    scratch_info.dstAccelerationStructure = current.as;
    scratch_info.scratchData.deviceAddress = get_device_address(scratch_buffer);

    vk::AccelerationStructureBuildRangeInfoKHR range_info{};
    range_info.primitiveCount = primitive_count;

    std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> range_infos = {&range_info};

    // vk::CommandBuffer cmd_buffer = pool.single_command_buffer(vk::CommandBufferLevel::ePrimary);
    vk::CommandBuffer cmd_buffer = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(
        general_command_pool, vk::CommandBufferLevel::ePrimary, 1)).front();
    cmd_buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        
    cmd_buffer.buildAccelerationStructuresKHR(scratch_info, range_infos, dl);
    cmd_buffer.end();

    auto q = device.getQueue(graphics_queue_family_index, 0);
    vk::SubmitInfo submit_info;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;
    q.submit(1, &submit_info, nullptr);
    q.waitIdle();
    device.freeCommandBuffers(general_command_pool, 1, &cmd_buffer);

    // pool.submit_command(command);
    // pool.free_command_buffer(command);

    // device.destroyBuffer(scratch.buffer);
    // device.freeMemory(scratch.memory);
    vmaDestroyBuffer(allocator, scratch_buffer, scratch_allocation);

    vk::AccelerationStructureDeviceAddressInfoKHR address_info{};
    address_info.accelerationStructure = current.as;
    current.as_addr = device.getAccelerationStructureAddressKHR(address_info, dl);

    tlas->blas[mesh] = current;
}

void Renderer::create_TLAS(TopLevelAccelerationStructure * tlas) {
    vk::BufferUsageFlags usage;
    
    std::vector<vk::AccelerationStructureInstanceKHR> instances;
    uint32_t instance_index = 0;
    for (auto& object: tlas->instance_buffers) {
        vk::TransformMatrixKHR transform = from_mat4(object.transformation);
        AccelerationBuffer current_blas = tlas->blas[object.mesh_buffer];

        vk::AccelerationStructureInstanceKHR instance{};
        instance.transform = transform;
        instance.instanceCustomIndex = instance_index++;
        instance.mask = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR; // maybe use vk::GeometryInstanceFlagBitsKHR::eTriangleFrontCounterclockwise
        instance.accelerationStructureReference = current_blas.as_addr;

        instances.push_back(instance);
    }
    // Create instance buffer
    usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress
    | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
    vk::DeviceSize size = instances.size() * sizeof(vk::AccelerationStructureInstanceKHR);
    // create_device_buffer(tlas.buffer, tlas.memory, instances.data(), size, usage);
    auto [as_buffer, as_allocation] = 
        create_device_buffer_with_data(instances.data(), size, usage);


    vk::DeviceOrHostAddressConstKHR instance_address{};
    instance_address.deviceAddress = get_device_address(as_buffer);

    vk::AccelerationStructureGeometryInstancesDataKHR geometry_data{};
    geometry_data.arrayOfPointers = VK_FALSE;
    geometry_data.data = instance_address;

    vk::AccelerationStructureGeometryKHR geometry{};
    geometry.geometryType = vk::GeometryTypeKHR::eInstances;
    geometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
    geometry.geometry.instances = geometry_data;

    vk::AccelerationStructureBuildGeometryInfoKHR geometry_info{};
    geometry_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;
    geometry_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    geometry_info.geometryCount = 1;
    geometry_info.pGeometries = &geometry;

    const uint32_t primitive_count = instances.size();

    // Create tlas buffer
    vk::AccelerationStructureBuildSizesInfoKHR size_info{};
    size_info = device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, geometry_info, primitive_count, dl);
    usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
    // create_buffer(tlas.as.buffer, tlas.as.memory, size_info.accelerationStructureSize, usage, vk::MemoryPropertyFlagBits::eDeviceLocal);
    auto [tlas_buffer, tlas_allocation] = 
        create_device_buffer(size_info.accelerationStructureSize, usage);
    tlas->buffer = tlas_buffer;
    tlas->allocation = tlas_allocation;

    vk::AccelerationStructureCreateInfoKHR create_info{};
    create_info.buffer = tlas->buffer;
    create_info.size = size_info.accelerationStructureSize;
    create_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;
    // tlas.as.as = device.createAccelerationStructureKHR(create_info, nullptr, dl);
    tlas->structure = device.createAccelerationStructureKHR(create_info, nullptr, dl);

    usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
    // AccelerationBuffer scratch{};  
    // create_buffer(scratch.buffer, scratch.memory, size_info.accelerationStructureSize, usage, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    auto [scratch_buffer, scratch_allocation] = 
        create_device_buffer(size_info.buildScratchSize, usage);

    vk::AccelerationStructureBuildGeometryInfoKHR scratch_info{};
    scratch_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;
    scratch_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;  //  note: add vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate for dynamic scenes
    scratch_info.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
    scratch_info.geometryCount = 1;
    scratch_info.pGeometries = &geometry;
    scratch_info.dstAccelerationStructure = tlas->structure;
    scratch_info.scratchData.deviceAddress = get_device_address(scratch_buffer);

    vk::AccelerationStructureBuildRangeInfoKHR range_info{};
    range_info.primitiveCount = primitive_count;

    std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> range_infos = {&range_info};
    // vk::CommandBuffer cmd_buffer = pool.single_command_buffer(vk::CommandBufferLevel::ePrimary);
    vk::CommandBuffer cmd_buffer = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(
        general_command_pool, vk::CommandBufferLevel::ePrimary, 1)).front();
    cmd_buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    cmd_buffer.buildAccelerationStructuresKHR(scratch_info, range_infos, dl);
    cmd_buffer.end();

    auto q = device.getQueue(graphics_queue_family_index, 0);
    vk::SubmitInfo submit_info;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;
    q.submit(1, &submit_info, nullptr);
    q.waitIdle();
    device.freeCommandBuffers(general_command_pool, 1, &cmd_buffer);

    // pool.submit_command(command);
    // pool.free_command_buffer(command);

    // note: keep scratch for dynamic scenes to avoid allocation overhead
    // device.destroyBuffer(scratch.buffer);
    // device.freeMemory(scratch.memory);
    vmaDestroyBuffer(allocator, scratch_buffer, scratch_allocation);

    vk::AccelerationStructureDeviceAddressInfoKHR address_info{};
    address_info.accelerationStructure = tlas->structure; //tlas.as.as;
    tlas->addr = device.getAccelerationStructureAddressKHR(address_info, dl);
}

void Renderer::load_scene(std::string file_path) {
    scene = Scene(file_path);

    tlas = std::make_unique<TopLevelAccelerationStructure>(device, dl, 
        general_command_pool, graphics_queue_family_index);
    auto & meshes = tlas->meshes;
    for (auto& object : scene) {
        // InstanceBuffer current_instance;
        auto it = meshes.find(object.mesh);
        if (it == meshes.end()) {
            create_mesh_buffer(tlas.get(), object.mesh);
            it = meshes.find(object.mesh);
            if (it == meshes.end()) {
                throw std::runtime_error("Failed to create mesh buffer");
            }

            create_BLAS(tlas.get(), &it->second);
        } 
        // create_device_buffer(current_instance.buffer, current_instance.memory, &object.transformation, sizeof(glm::mat4), 
        //                                                                                     vk::BufferUsageFlagBits::eVertexBuffer);
        // current_instance.mesh_buffer = &it->second;
        // current_instance.transformation = object.transformation;
        // objects.push_back(current_instance);
        tlas->instance_buffers.emplace_back(InstanceBuffer(&it->second, object.transformation));
    }

    create_TLAS(tlas.get());
}

void Renderer::create_sbt() {
    vk::PhysicalDeviceProperties2 properties;
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rt_properties;

    rt_properties.sType = vk::StructureType::ePhysicalDeviceRayTracingPipelinePropertiesKHR;
    properties.sType = vk::StructureType::ePhysicalDeviceProperties2;
    properties.pNext = &rt_properties;
    physical_device.getProperties2(&properties);

    const uint32_t handle_size = rt_properties.shaderGroupHandleSize;
    const uint32_t handle_alignment = rt_properties.shaderGroupHandleAlignment;
    const uint32_t handle_size_aligned = (handle_size+(handle_alignment-1))&~(handle_alignment-1);
    const uint32_t group_count = 3;
    const uint32_t sbt_size = group_count * handle_size_aligned*2;
    const vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eShaderDeviceAddress;


    // Debuggin: print sbt information
    std::cout << "handle_size: " << handle_size << std::endl;
    std::cout << "handle_alignment: " << handle_alignment << std::endl;
    std::cout << "handle_size_aligned: " << handle_size_aligned << std::endl;
    std::cout << "sbt_size: " << sbt_size << std::endl;
    std::cout << "group_count: " << group_count << std::endl;

    std::vector<uint8_t> handle_storage(sbt_size);

    device.getRayTracingShaderGroupHandlesKHR(pipeline->pipeline, 0, group_count, sbt_size, handle_storage.data(), dl);

    std::vector<uint8_t> sbt_data(sbt_size);
    std::memcpy(sbt_data.data(), handle_storage.data(), handle_size_aligned); // raygen
    std::memcpy(sbt_data.data() + handle_size_aligned*2, handle_storage.data() + handle_size_aligned, handle_size_aligned); // miss
    std::memcpy(sbt_data.data() + handle_size_aligned*4, handle_storage.data() + handle_size_aligned*2, handle_size_aligned); // hit

    auto [sbt_buffer, sbt_allocation] = create_device_buffer_with_data(sbt_data.data(), sbt_data.size(), usage);
    sbt.buffer = sbt_buffer;
    sbt.allocation = sbt_allocation;

    auto sbt_address = get_device_address(sbt.buffer);


    sbt.raygen_region = vk::StridedDeviceAddressRegionKHR(
        sbt_address, handle_size_aligned, handle_size_aligned
    );

    sbt.miss_region = vk::StridedDeviceAddressRegionKHR(
        sbt_address + handle_size_aligned*2, handle_size_aligned, handle_size_aligned
    );

    sbt.hit_region = vk::StridedDeviceAddressRegionKHR(
        sbt_address + handle_size_aligned*4, handle_size_aligned, handle_size_aligned
    );

    sbt.callable_region = vk::StridedDeviceAddressRegionKHR();

    std::cout << "Created SBTs" << std::endl;
}