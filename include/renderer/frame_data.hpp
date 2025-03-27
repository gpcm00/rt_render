#pragma once
#include <renderer/vulkan.hpp>
#include <memory>

// Contains data common to all frames
class CommonFrameData {
private:

vk::Device & device;
size_t num_frames;

vk::CommandPool command_pool;

friend class FrameData;

public:

    CommonFrameData(vk::Device & device, size_t num_frames, int graphics_queue_family_index): 
    device(device), num_frames(num_frames) {

        vk::CommandPoolCreateInfo pool_info{};
        pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        pool_info.queueFamilyIndex = graphics_queue_family_index; // assumes graphics and present queue family are the same

        command_pool = device.createCommandPool(pool_info);
    }

    ~CommonFrameData() {

        device.destroyCommandPool(command_pool);

    }
};

// Contains frame-specific Vulkan data
// wrt. the swapchain images (e.g. 3 frames in flight)
class FrameData {
private:

std::shared_ptr<CommonFrameData> common_data;
vk::Device & device;
int frame_index;

vk::CommandBuffer command_buffer;

public:

FrameData(std::shared_ptr<CommonFrameData> common_data, int frame_index): 
device(common_data->device), frame_index(frame_index) {

    vk::CommandBufferAllocateInfo info{};
    info.level = vk::CommandBufferLevel::ePrimary;
    info.commandPool = common_data->command_pool;
    info.commandBufferCount = 1;

    command_buffer = device.allocateCommandBuffers(info).front();
}

~FrameData() {

    device.freeCommandBuffers(common_data->command_pool, command_buffer);
}

};