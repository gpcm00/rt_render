#include <renderer/renderer.hpp>

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
