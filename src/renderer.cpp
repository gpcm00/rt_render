#include <renderer/renderer.hpp>

static vk::TransformMatrixKHR from_mat4(const glm::mat4 &mat) {
    // glm::mat4 should be column major but vk::TransformMatrixKHR appears to be
    // row major
    vk::TransformMatrixKHR ret{};
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            ret.matrix[r][c] = mat[c][r];
        }
    }
    return ret;
}

static vk::WriteDescriptorSet populate_write_descriptor(vk::Buffer buffer,
                                                        vk::DeviceSize size,
                                                        vk::DescriptorSet set,
                                                        vk::DescriptorType type,
                                                        uint32_t binding) {
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

void Renderer::create_buffer(vk::Buffer &buffer, vk::DeviceMemory &memory,
                             vk::DeviceSize size, vk::BufferUsageFlags usage,
                             vk::MemoryPropertyFlags properties,
                             vk::SharingMode mode) {
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
        if ((requirements.memoryTypeBits & (1 << mem_idx)) &&
            ((mem_properties.memoryTypes[mem_idx].propertyFlags & properties) ==
             properties)) {
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

void Renderer::create_mesh_buffer(TopLevelAccelerationStructure *tlas,
                                  const Primitive *mesh) {
    MeshBuffer mesh_buffer;

    vk::BufferUsageFlags flags =
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
        vk::BufferUsageFlagBits::eShaderDeviceAddress;

    // Vertex buffer
    auto [vertex_buffer, vertex_allocation] = create_device_buffer_with_data(
        mesh->vertices.data(), mesh->vertices.size() * sizeof(Vertex), flags);
    mesh_buffer.vertex_buffer = vertex_buffer;
    mesh_buffer.vertex_allocation = vertex_allocation;
    // Index buffer

    auto [index_buffer, index_allocation] = create_device_buffer_with_data(
        mesh->indices.data(), mesh->indices.size() * sizeof(uint32_t), flags);
    mesh_buffer.index_buffer = index_buffer;
    mesh_buffer.index_allocation = index_allocation;

    mesh_buffer.num_vertices = mesh->vertices.size();
    mesh_buffer.num_indices = mesh->indices.size();
    tlas->meshes[mesh] = mesh_buffer;
}

void Renderer::create_BLAS(TopLevelAccelerationStructure *tlas,
                           const MeshBuffer *mesh) {
    vk::BufferUsageFlags usage;

    vk::AccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
    triangles.vertexData.deviceAddress =
        get_device_address(mesh->vertex_buffer);
    triangles.vertexStride = sizeof(Vertex);
    triangles.indexType = vk::IndexType::eUint32;
    triangles.indexData.deviceAddress = get_device_address(mesh->index_buffer);
    // triangles.maxVertex = mesh->num_vertices - 1;
    vk::AccelerationStructureGeometryKHR geometry{};
    geometry.geometryType = vk::GeometryTypeKHR::eTriangles;
    geometry.geometry.triangles = triangles;
    geometry.flags = vk::GeometryFlagBitsKHR::eOpaque;

    vk::AccelerationStructureBuildGeometryInfoKHR info{};
    info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    info.geometryCount = 1;
    info.pGeometries = &geometry;

    const uint32_t primitive_count = mesh->num_indices / 3; // mesh->size / 3;
    vk::AccelerationStructureBuildSizesInfoKHR size_info{};
    size_info = device.getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice, info, primitive_count,
        dl);

    usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
            vk::BufferUsageFlagBits::eShaderDeviceAddress;
    AccelerationBuffer current{};
    auto [current_buffer, current_allocation] =
        create_device_buffer(size_info.accelerationStructureSize, usage);
    current.buffer = current_buffer;
    current.allocation = current_allocation;

    vk::AccelerationStructureCreateInfoKHR create_info{};
    create_info.buffer = current_buffer;
    create_info.size = size_info.accelerationStructureSize;
    create_info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    current.as =
        device.createAccelerationStructureKHR(create_info, nullptr, dl);
    usage = vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eShaderDeviceAddress;
    auto [scratch_buffer, scratch_allocation] =
        create_device_buffer(size_info.buildScratchSize, usage);

    vk::AccelerationStructureBuildGeometryInfoKHR scratch_info{};
    scratch_info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    scratch_info.flags =
        vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    scratch_info.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
    scratch_info.geometryCount = 1;
    scratch_info.pGeometries = &geometry;
    scratch_info.dstAccelerationStructure = current.as;
    scratch_info.scratchData.deviceAddress = get_device_address(scratch_buffer);

    vk::AccelerationStructureBuildRangeInfoKHR range_info{};
    range_info.primitiveCount = primitive_count;

    std::vector<vk::AccelerationStructureBuildRangeInfoKHR *> range_infos = {
        &range_info};

    vk::CommandBuffer cmd_buffer =
        device
            .allocateCommandBuffers(vk::CommandBufferAllocateInfo(
                general_command_pool, vk::CommandBufferLevel::ePrimary, 1))
            .front();
    cmd_buffer.begin(vk::CommandBufferBeginInfo(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    cmd_buffer.buildAccelerationStructuresKHR(scratch_info, range_infos, dl);
    cmd_buffer.end();

    auto q = device.getQueue(graphics_queue_family_index, 0);
    vk::SubmitInfo submit_info;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;
    q.submit(1, &submit_info, nullptr);
    q.waitIdle();
    device.freeCommandBuffers(general_command_pool, 1, &cmd_buffer);

    vmaDestroyBuffer(allocator, scratch_buffer, scratch_allocation);

    vk::AccelerationStructureDeviceAddressInfoKHR address_info{};
    address_info.accelerationStructure = current.as;
    current.as_addr =
        device.getAccelerationStructureAddressKHR(address_info, dl);

    tlas->blas[mesh] = current;
}

void Renderer::create_TLAS(TopLevelAccelerationStructure *tlas) {
    vk::BufferUsageFlags usage;

    std::vector<vk::AccelerationStructureInstanceKHR> instances;
    for (auto &object : tlas->instance_buffers) {
        vk::TransformMatrixKHR transform = from_mat4(object.transformation);
        AccelerationBuffer current_blas = tlas->blas[object.mesh_buffer];

        vk::AccelerationStructureInstanceKHR instance{};
        instance.transform = transform;
        instance.instanceCustomIndex = object.instance_id;
        instance.mask = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags =
            VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = current_blas.as_addr;

        instances.push_back(instance);
    }
    // Create instance buffer
    usage =
        vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
        vk::BufferUsageFlagBits::eShaderDeviceAddress |
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
    vk::DeviceSize size =
        instances.size() * sizeof(vk::AccelerationStructureInstanceKHR);
    auto [as_buffer, as_allocation] =
        create_device_buffer_with_data(instances.data(), size, usage);
    tlas->tlas_instance_buffer = as_buffer;
    tlas->tlas_instance_allocation = as_allocation;

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
    geometry_info.flags =
        vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    geometry_info.geometryCount = 1;
    geometry_info.pGeometries = &geometry;

    const uint32_t primitive_count = instances.size();

    // Create tlas buffer
    vk::AccelerationStructureBuildSizesInfoKHR size_info{};
    size_info = device.getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice, geometry_info,
        primitive_count, dl);
    usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
            vk::BufferUsageFlagBits::eShaderDeviceAddress;
    auto [tlas_buffer, tlas_allocation] =
        create_device_buffer(size_info.accelerationStructureSize, usage);
    tlas->buffer = tlas_buffer;
    tlas->allocation = tlas_allocation;

    vk::AccelerationStructureCreateInfoKHR create_info{};
    create_info.buffer = tlas->buffer;
    create_info.size = size_info.accelerationStructureSize;
    create_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;
    tlas->structure =
        device.createAccelerationStructureKHR(create_info, nullptr, dl);

    usage = vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eShaderDeviceAddress;
    auto [scratch_buffer, scratch_allocation] =
        create_device_buffer(size_info.buildScratchSize, usage);

    vk::AccelerationStructureBuildGeometryInfoKHR scratch_info{};
    scratch_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;
    scratch_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::
        ePreferFastTrace; //  note: add
                          //  vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate
                          //  for dynamic scenes
    scratch_info.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
    scratch_info.geometryCount = 1;
    scratch_info.pGeometries = &geometry;
    scratch_info.dstAccelerationStructure = tlas->structure;
    scratch_info.scratchData.deviceAddress = get_device_address(scratch_buffer);

    vk::AccelerationStructureBuildRangeInfoKHR range_info{};
    range_info.primitiveCount = primitive_count;
    range_info.primitiveOffset = 0;

    std::vector<vk::AccelerationStructureBuildRangeInfoKHR *> range_infos = {
        &range_info};
    vk::CommandBuffer cmd_buffer =
        device
            .allocateCommandBuffers(vk::CommandBufferAllocateInfo(
                general_command_pool, vk::CommandBufferLevel::ePrimary, 1))
            .front();
    cmd_buffer.begin(vk::CommandBufferBeginInfo(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    cmd_buffer.buildAccelerationStructuresKHR(scratch_info, range_infos, dl);
    cmd_buffer.end();

    auto q = device.getQueue(graphics_queue_family_index, 0);
    vk::SubmitInfo submit_info;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;
    q.submit(1, &submit_info, nullptr);
    q.waitIdle();
    device.freeCommandBuffers(general_command_pool, 1, &cmd_buffer);

    vmaDestroyBuffer(allocator, scratch_buffer, scratch_allocation);

    vk::AccelerationStructureDeviceAddressInfoKHR address_info{};
    address_info.accelerationStructure = tlas->structure; // tlas.as.as;
    tlas->addr = device.getAccelerationStructureAddressKHR(address_info, dl);
}

void Renderer::load_scene(std::string file_path) {
    scene = std::make_unique<Scene>(file_path);

    tlas = std::make_unique<TopLevelAccelerationStructure>(
        device, allocator, dl, general_command_pool,
        graphics_queue_family_index);
    auto &meshes = tlas->meshes; // it's called meshes but it holds primitive
    tlas->mesh_data.resize(scene->num_primitives()); // also per-primitive data

    for (auto &object : *scene) {
        for (auto &primitive : object.mesh->primitives) {
            auto it = meshes.find(&primitive);
            if (it == meshes.end()) {
                create_mesh_buffer(tlas.get(), &primitive);
                it = meshes.find(&primitive);
                if (it == meshes.end()) {
                    throw std::runtime_error("Failed to create mesh buffer");
                }

                if (primitive.material_index == -1) {
                    std::cout << "Warning: primitive " << primitive.primitive_id
                              << " has no material" << std::endl;
                } else if (primitive.material_index >= scene->material_size()) {
                    std::cout << "Warning: primitive " << primitive.primitive_id
                              << " has invalid material index" << std::endl;
                }

                tlas->mesh_data[primitive.primitive_id] = MeshData{
                    get_device_address(it->second.vertex_buffer),
                    get_device_address(it->second.index_buffer),
                    static_cast<uint32_t>(
                        std::max(0, primitive.material_index)),
                };

                create_BLAS(tlas.get(), &it->second);
            }

            tlas->instance_buffers.emplace_back(
                InstanceBuffer(&it->second, object.global_transformation,
                               tlas->instance_data.size()));
            tlas->instance_data.push_back(InstanceData{primitive.primitive_id});
        }
    }

    // Create instance data buffer
    auto [instance_data_buffer, instance_data_allocation] =
        create_device_buffer_with_data(
            tlas->instance_data.data(),
            tlas->instance_data.size() * sizeof(InstanceData),
            vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eShaderDeviceAddress);
    tlas->instance_data_buffer = instance_data_buffer;
    tlas->instance_data_allocation = instance_data_allocation;

    // create mesh data buffer
    auto [mesh_data_buffer, mesh_data_allocation] =
        create_device_buffer_with_data(
            tlas->mesh_data.data(), tlas->mesh_data.size() * sizeof(MeshData),
            vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eShaderDeviceAddress);
    tlas->mesh_data_buffer = mesh_data_buffer;
    tlas->mesh_data_allocation = mesh_data_allocation;

    create_TLAS(tlas.get());

    // Upload texture data
    create_textures();
}

void Renderer::create_sbt() {
    vk::PhysicalDeviceProperties2 properties;
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rt_properties;

    rt_properties.sType =
        vk::StructureType::ePhysicalDeviceRayTracingPipelinePropertiesKHR;
    properties.sType = vk::StructureType::ePhysicalDeviceProperties2;
    properties.pNext = &rt_properties;
    physical_device.getProperties2(&properties);

    const uint32_t handle_size = rt_properties.shaderGroupHandleSize;
    const uint32_t handle_alignment = rt_properties.shaderGroupHandleAlignment;
    const uint32_t handle_size_aligned =
        (handle_size + (handle_alignment - 1)) & ~(handle_alignment - 1);
    const uint32_t group_count = 3;
    const uint32_t sbt_size = group_count * handle_size_aligned * 2;
    const vk::BufferUsageFlags usage =
        vk::BufferUsageFlagBits::eShaderBindingTableKHR |
        vk::BufferUsageFlagBits::eTransferSrc |
        vk::BufferUsageFlagBits::eShaderDeviceAddress;

    // Debuggin: print sbt information
    std::cout << "handle_size: " << handle_size << std::endl;
    std::cout << "handle_alignment: " << handle_alignment << std::endl;
    std::cout << "handle_size_aligned: " << handle_size_aligned << std::endl;
    std::cout << "sbt_size: " << sbt_size << std::endl;
    std::cout << "group_count: " << group_count << std::endl;

    std::vector<uint8_t> handle_storage(sbt_size);

    device.getRayTracingShaderGroupHandlesKHR(pipeline->pipeline, 0,
                                              group_count, sbt_size,
                                              handle_storage.data(), dl);

    std::vector<uint8_t> sbt_data(sbt_size);
    std::memcpy(sbt_data.data(), handle_storage.data(),
                handle_size_aligned); // raygen
    std::memcpy(sbt_data.data() + handle_size_aligned * 2,
                handle_storage.data() + handle_size_aligned,
                handle_size_aligned); // miss
    std::memcpy(sbt_data.data() + handle_size_aligned * 4,
                handle_storage.data() + handle_size_aligned * 2,
                handle_size_aligned); // hit

    auto [sbt_buffer, sbt_allocation] =
        create_device_buffer_with_data(sbt_data.data(), sbt_data.size(), usage);
    sbt.buffer = sbt_buffer;
    sbt.allocation = sbt_allocation;

    auto sbt_address = get_device_address(sbt.buffer);

    sbt.raygen_region = vk::StridedDeviceAddressRegionKHR(
        sbt_address, handle_size_aligned, handle_size_aligned);

    sbt.miss_region = vk::StridedDeviceAddressRegionKHR(
        sbt_address + handle_size_aligned * 2, handle_size_aligned,
        handle_size_aligned);

    sbt.hit_region = vk::StridedDeviceAddressRegionKHR(
        sbt_address + handle_size_aligned * 4, handle_size_aligned,
        handle_size_aligned);

    sbt.callable_region = vk::StridedDeviceAddressRegionKHR();

    std::cout << "Created SBTs" << std::endl;
}
