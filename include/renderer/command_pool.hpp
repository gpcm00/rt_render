#pragma once
#include <renderer/vulkan.hpp>

#include <iostream>
#include <vector>


class Command_Pool {
    vk::Device *device;
    vk::CommandPool pool;
    vk::Queue queue;
    bool initialized = false;

  public:
    Command_Pool() = default;
    Command_Pool(vk::Device *dev, vk::PhysicalDevice *physical_device,
                 vk::QueueFlagBits flag)
        : device(dev) {
        initialized = false;
        uint32_t count = 0;
        physical_device->getQueueFamilyProperties(&count, nullptr);

        std::vector<vk::QueueFamilyProperties> queue_families(count);
        physical_device->getQueueFamilyProperties(&count,
                                                  queue_families.data());

        uint32_t queue_index = 0;
        for (const auto &queue_family : queue_families) {
            if (queue_family.queueFlags & flag) {
                break;
            }
            queue_index++;
        }

        vk::CommandPoolCreateInfo poolInfo;
        poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        poolInfo.queueFamilyIndex = queue_index;

        pool = device->createCommandPool(poolInfo);

        queue = device->getQueue(queue_index, 0);
        initialized = true;
    }

    void destroy_pool() {
        if (initialized) {
            device->destroyCommandPool(pool);
            initialized = false;
        }
    }

    ~Command_Pool() { destroy_pool(); }

    vk::CommandBuffer single_command_buffer(vk::CommandBufferLevel level) {
        vk::CommandBufferAllocateInfo info{};
        info.level = vk::CommandBufferLevel::ePrimary;
        info.commandPool = pool;
        info.commandBufferCount = 1;

        return device->allocateCommandBuffers(info).front();
    }

    std::vector<vk::CommandBuffer>
    alloc_command_buffers(vk::CommandBufferLevel level, uint32_t count) {
        vk::CommandBufferAllocateInfo info{};
        info.level = level;
        info.commandPool = pool;
        info.commandBufferCount = count;

        return device->allocateCommandBuffers(info);
    }

    void submit_command(vk::CommandBuffer command_buffer) {
        vk::SubmitInfo info{};
        info.commandBufferCount = 1;
        info.pCommandBuffers = &command_buffer;

        queue.submit(info, nullptr);
        queue.waitIdle();
    }

    void submit_commands(std::vector<vk::CommandBuffer> command_buffers) {
        // needs to change to use fences instead of wait idle so the program
        // does not block
        // TODO: add fence logic

        vk::SubmitInfo info{};
        info.commandBufferCount = command_buffers.size();
        info.pCommandBuffers = command_buffers.data();

        queue.submit(info, nullptr);

        // queue.waitIdle();
    }

    void free_command_buffer(vk::CommandBuffer command_buffer) {
        device->freeCommandBuffers(pool, command_buffer);
    }

    void free_command_buffer(std::vector<vk::CommandBuffer> &command_buffers) {
        device->freeCommandBuffers(pool, command_buffers);
    }
};
