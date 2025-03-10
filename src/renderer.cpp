#include <renderer/renderer.hpp>

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

void Renderer::create_device_buffer(VkBuffer& buffer, VkDeviceMemory& memory, const void* host_buffer, VkDeviceSize size, VkBufferUsageFlagBits usage) {
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    create_buffer(staging_buffer, staging_buffer_memory, size, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(device, staging_buffer_memory, 0, size, 0, &data);
    memcpy(data, host_buffer, (size_t)size);
    vkUnmapMemory(device, staging_buffer_memory);

    create_buffer(buffer, memory, size, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Command_Pool pool((const VkDevice*)&device, (VkPhysicalDevice)physical_device, (VkQueueFlagBits)(vk::QueueFlagBits::eGraphics));

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

    const void* data = mesh->vertices.data();
    VkDeviceSize size = mesh->vertices.size() * sizeof(Vertex);
    create_device_buffer(mesh_buffer.vertex_buffer, mesh_buffer.vertex_memory, data, size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    data =  mesh->indices.data();
    size = mesh->indices.size() * sizeof(uint32_t);
    create_device_buffer(mesh_buffer.vertex_buffer, mesh_buffer.vertex_memory, data, size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    mesh_buffer.size = size;
    meshes[mesh] = mesh_buffer;
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
                throw std::runtime_error("Failed to create mesh beffer");
            }
        } 
        create_device_buffer(current_instance.buffer, current_instance.memory, &object.transformation, sizeof(glm::mat4), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        current_instance.mesh_buffer = &it->second;
        objects.push_back(current_instance);
    }
}