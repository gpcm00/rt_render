#pragma once
#include <renderer/vulkan.hpp>
#include <memory>

// Contains data common to all frames
class CommonFrameData {
private:

vk::Device & device;
VmaAllocator & allocator;

size_t num_frames;

vk::CommandPool command_pool;

friend class FrameData;

public:

    CommonFrameData(vk::Device & device, VmaAllocator & allocator, size_t num_frames, int graphics_queue_family_index): 
    device(device), allocator(allocator), num_frames(num_frames) {

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
int width;
int height;
int frame_index;

public:

vk::CommandBuffer command_buffer;
vk::Fence fence;

// RT output
vk::Image rt_image;
vk::ImageView rt_image_view;
VmaAllocation rt_image_allocation;

// We'll later need more images for denoising and for output which
// gets blitted to the swapchain image

// Semaphores for synchronization
vk::Semaphore sem;


FrameData(std::shared_ptr<CommonFrameData> common_data, int width, int height, int frame_index): 
device(common_data->device), width(width), height(height), frame_index(frame_index) {

    vk::CommandBufferAllocateInfo info{};
    info.level = vk::CommandBufferLevel::ePrimary;
    info.commandPool = common_data->command_pool;
    info.commandBufferCount = 1;

    command_buffer = device.allocateCommandBuffers(info).front();

    vk::FenceCreateInfo fence_info;
    fence_info.flags = vk::FenceCreateFlagBits::eSignaled; // so that the first wait on empty work doesn't fail
    fence = device.createFence(fence_info);

    // create an image for the ray tracing output
    // vk::ImageCreateInfo image_info{};
    // image_info.imageType = vk::ImageType::e2D;
    // image_info.extent.width = width;
    // image_info.extent.height = height;
    // image_info.extent.depth = 1;
    // image_info.mipLevels = 1;
    // image_info.arrayLayers = 1;
    // image_info.format = vk::Format::eR8G8B8A8Unorm;
    // image_info.tiling = vk::ImageTiling::eOptimal;
    // image_info.initialLayout = vk::ImageLayout::eUndefined;
    // image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc;
    // image_info.samples = vk::SampleCountFlagBits::e1;
    // image_info.sharingMode = vk::SharingMode::eExclusive;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT 
        | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    vmaCreateImage(common_data->allocator, &image_info, &alloc_info, reinterpret_cast<VkImage*>(&rt_image), &rt_image_allocation, nullptr);

    // Create image view
    vk::ImageViewCreateInfo view_info{};
    view_info.image = rt_image;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = vk::Format::eR8G8B8A8Unorm;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    rt_image_view = device.createImageView(view_info);

    // Create semaphore
    vk::SemaphoreCreateInfo sem_info{};
    device.createSemaphore(&sem_info, nullptr, &sem);
}

~FrameData() {

    device.destroySemaphore(sem);
    device.destroyImageView(rt_image_view);
    vmaDestroyImage(common_data->allocator, rt_image, rt_image_allocation);
    
    device.destroyFence(fence);
    device.freeCommandBuffers(common_data->command_pool, command_buffer);
}

};