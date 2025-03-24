#pragma once
#include <vulkan/vulkan.hpp>

#include <vector>
#include <iostream>

class Command_Pool {
    const VkDevice* device;
    VkCommandPool pool;
    VkQueue queue;

    public:
    Command_Pool() = default;
    Command_Pool(const VkDevice* dev, VkPhysicalDevice physical_device, VkQueueFlagBits flag) : device(dev) {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queue_families.data());

        uint32_t queue_index = 0;
        for (const auto& queue_family : queue_families) {
            if (queue_family.queueFlags & flag) {
                break;
            }
            queue_index++;
        }

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queue_index;

        if (vkCreateCommandPool(*device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics command pool!");
        }

        vkGetDeviceQueue(*device, queue_index, 0, &queue);
    }

    ~Command_Pool() {
        vkDestroyCommandPool(*device, pool, nullptr);
    }

    VkCommandBuffer single_command_buffer(VkCommandBufferLevel level) {
        VkCommandBufferAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandPool = pool;
        info.commandBufferCount = 1;

        VkCommandBuffer command_buffer;
        if (vkAllocateCommandBuffers(*device, &info, &command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate single command buffer");
        }

        return command_buffer;
    }

    std::vector<VkCommandBuffer> alloc_command_buffers(VkCommandBufferLevel level, uint32_t count, VkCommandBufferLevel level) {
        std::vector<VkCommandBuffer> buffers(count);

        VkCommandBufferAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.level = level;
        info.commandPool = pool;
        info.commandBufferCount = count;

        if (vkAllocateCommandBuffers(*device, &info, buffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers");
        }

        return buffers;
    }

    void submit_command(VkCommandBuffer command_buffer) {
        VkSubmitInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &command_buffer;

        if (vkQueueSubmit(queue, 1, &info, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit command buffer");
        }

        vkQueueWaitIdle(queue);
    }

    void submit_commands(std::vector<VkCommandBuffer> command_buffers) {
        // needs to change to use fences instead of wait idle so the progrma does no block
        // TODO: add fence logic
        
        VkSubmitInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.commandBufferCount = command_buffers.size();
        info.pCommandBuffers = command_buffers.data();

        if (vkQueueSubmit(queue, 1, &info, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit command buffer");
        }
        
        // vkQueueWaitIdle(queue);
    }

    void free_command_buffer(VkCommandBuffer command_buffer) {
        vkFreeCommandBuffers(*device, pool, 1, &command_buffer);
    }

    void free_command_buffer(std::vector<VkCommandBuffer>& command_buffers) {
        vkFreeCommandBuffers(*device, pool, command_buffers.size(), command_buffers.data());
    }
};