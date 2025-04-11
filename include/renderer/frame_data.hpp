#pragma once
#include <memory>
#include <renderer/camera.hpp>
#include <renderer/vulkan.hpp>


// Contains data common to all frames
class CommonFrameData {
  private:
    vk::Device &device;
    VmaAllocator &allocator;

    size_t num_frames;

    vk::CommandPool command_pool;

    friend class FrameData;

  public:
    std::vector<vk::DescriptorSetLayout> descriptor_set_layouts;

    CommonFrameData(vk::Device &device, VmaAllocator &allocator,
                    size_t num_frames, int graphics_queue_family_index)
        : device(device), allocator(allocator), num_frames(num_frames) {

        vk::CommandPoolCreateInfo pool_info{};
        pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        pool_info.queueFamilyIndex =
            graphics_queue_family_index; // assumes graphics and present queue
                                         // family are the same

        command_pool = device.createCommandPool(pool_info);
    }

    ~CommonFrameData() {

        for (auto &layout : descriptor_set_layouts) {
            device.destroyDescriptorSetLayout(layout);
        }
        descriptor_set_layouts.clear();

        device.destroyCommandPool(command_pool);
    }
};

// Contains frame-specific Vulkan data
// wrt. the swapchain images (e.g. 3 frames in flight)
class FrameData {
  private:
    std::shared_ptr<CommonFrameData> common_data;
    vk::Device &device;
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

    // Camera UBO
    vk::Buffer camera_buffer;
    VmaAllocation camera_allocation;

    // Persistant staging buffer
    vk::Buffer staging_buffer;
    VmaAllocation staging_buffer_allocation;

    // We'll later need more images for denoising and for output which
    // gets blitted to the swapchain image

    // Semaphores for synchronization
    vk::Semaphore sem;
    vk::Semaphore sc_image_available;

    // Descriptor set nonsense
    // TODO: Try descriptor buffers later
    vk::DescriptorPool descriptor_pool;
    std::unordered_map<vk::DescriptorSetLayout, vk::DescriptorSet>
        descriptor_sets;

    bool camera_changed;
    uint32_t sample_index;
    ;

    FrameData(std::shared_ptr<CommonFrameData> common_data, int width,
              int height, int frame_index)
        : common_data(common_data), device(common_data->device), width(width),
          height(height), frame_index(frame_index), camera_changed(true) {

        vk::CommandBufferAllocateInfo info{};
        info.level = vk::CommandBufferLevel::ePrimary;
        info.commandPool = common_data->command_pool;
        info.commandBufferCount = 1;

        command_buffer = device.allocateCommandBuffers(info).front();

        vk::FenceCreateInfo fence_info;
        fence_info.flags =
            vk::FenceCreateFlagBits::eSignaled; // so that the first wait on
                                                // empty work doesn't fail
        fence = device.createFence(fence_info);

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
        image_info.usage = VK_IMAGE_USAGE_STORAGE_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        vmaCreateImage(common_data->allocator, &image_info, &alloc_info,
                       reinterpret_cast<VkImage *>(&rt_image),
                       &rt_image_allocation, nullptr);

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

        // Create camera buffer
        vk::BufferCreateInfo buffer_info{};
        buffer_info.size = sizeof(RTCamera);
        buffer_info.usage = vk::BufferUsageFlagBits::eUniformBuffer |
                            vk::BufferUsageFlagBits::eTransferDst;
        buffer_info.sharingMode = vk::SharingMode::eExclusive;

        VmaAllocationCreateInfo buffer_alloc_info{};
        buffer_alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        buffer_alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        buffer_alloc_info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        vmaCreateBuffer(common_data->allocator,
                        reinterpret_cast<VkBufferCreateInfo *>(&buffer_info),
                        &buffer_alloc_info,
                        reinterpret_cast<VkBuffer *>(&camera_buffer),
                        &camera_allocation, nullptr);

        // Create reusable staging buffer
        vk::BufferCreateInfo staging_buffer_info{};
        staging_buffer_info.size = 1024 * 1024; // 1MB
        staging_buffer_info.usage = vk::BufferUsageFlagBits::eTransferSrc |
                                    vk::BufferUsageFlagBits::eTransferDst;
        staging_buffer_info.sharingMode = vk::SharingMode::eExclusive;

        VmaAllocationCreateInfo staging_buffer_alloc_info{};
        staging_buffer_alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        staging_buffer_alloc_info.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;

        vmaCreateBuffer(
            common_data->allocator,
            reinterpret_cast<VkBufferCreateInfo *>(&staging_buffer_info),
            &staging_buffer_alloc_info,
            reinterpret_cast<VkBuffer *>(&staging_buffer),
            &staging_buffer_allocation, nullptr);

        // Create semaphore
        vk::SemaphoreCreateInfo sem_info{};
        device.createSemaphore(&sem_info, nullptr, &sem);
        device.createSemaphore(&sem_info, nullptr, &sc_image_available);

        // Descriptor pool creation
        // We'll try using one pool per frame for all pipeline layouts
        vk::DescriptorPoolSize pool_size{};
        pool_size.type = vk::DescriptorType::eAccelerationStructureKHR;
        pool_size.descriptorCount = 1;

        vk::DescriptorPoolSize pool_size2{};
        pool_size2.type = vk::DescriptorType::eStorageImage;
        pool_size2.descriptorCount = 1;

        vk::DescriptorPoolSize pool_size3{};
        pool_size3.type = vk::DescriptorType::eCombinedImageSampler;
        pool_size3.descriptorCount = 1024;

        const auto pool_sizes = std::array{pool_size, pool_size2, pool_size3};

        vk::DescriptorPoolCreateInfo descriptor_pool_info{};
        descriptor_pool_info.maxSets = 10;
        descriptor_pool_info.poolSizeCount = 3;
        descriptor_pool_info.pPoolSizes = pool_sizes.data();

        device.createDescriptorPool(&descriptor_pool_info, nullptr,
                                    &descriptor_pool);

        // Now create a descriptor
        sample_index = 0;
    }

    ~FrameData() {

        device.destroyDescriptorPool(descriptor_pool);

        device.destroySemaphore(sc_image_available);
        device.destroySemaphore(sem);
        device.destroyImageView(rt_image_view);
        vmaDestroyBuffer(common_data->allocator, camera_buffer,
                         camera_allocation);
        vmaDestroyBuffer(common_data->allocator, staging_buffer,
                         staging_buffer_allocation);
        vmaDestroyImage(common_data->allocator, rt_image, rt_image_allocation);

        device.destroyFence(fence);
        device.freeCommandBuffers(common_data->command_pool, command_buffer);
    }
};
