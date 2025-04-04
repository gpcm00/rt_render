#pragma once
#include <renderer/vulkan.hpp>

#include <renderer/swapchain.hpp>
#include <renderer/command_pool.hpp>
#include <renderer/pipeline.hpp>
#include <renderer/window/window_system_glfw.hpp>

#include <geometry/geometry.hpp>
#include <renderer/frame_constants.hpp>
#include <renderer/frame_data.hpp>
#include <renderer/rt_pipeline.hpp>
#include <renderer/acceleration_structure.hpp>
#include <renderer/image.hpp>

#include <unordered_map>
#include <memory>


class ShaderBindingTable {
    public:
    vk::Buffer buffer;
    VmaAllocation allocation;
    vk::StridedDeviceAddressRegionKHR raygen_region;
    vk::StridedDeviceAddressRegionKHR miss_region;
    vk::StridedDeviceAddressRegionKHR hit_region;
    vk::StridedDeviceAddressRegionKHR callable_region;
};




class Renderer {
    private:
    // hard code the dimensions for now
    static constexpr int r_width = 1280;
    static constexpr int r_height = 720;
    public:

    std::pair<int, int> get_dimensions() {
        return {r_width, r_height};
    }

    private:
    WindowHandle window;
    WindowSystemGLFW * window_system;

    vk::detail::DispatchLoaderDynamic dl;
    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    vk::Device device;

    int graphics_queue_family_index;
    int present_queue_family_index;

    vk::SurfaceKHR surface;
    std::unique_ptr<Swapchain> swapchain;

    // std::unordered_map<const Mesh*, MeshBuffer> meshes{};
    // std::vector<InstanceBuffer> objects{};

    // Command_Pool pool;
    vk::CommandPool general_command_pool;
    // Pipeline pipeline;

    std::unique_ptr<Scene> scene;

    // UnifromBuffer camera;
    RTCamera camera;

    ShaderBindingTable sbt;

    ImageStorage images;


    // std::unordered_map<const MeshBuffer*, AccelerationBuffer> blas;
    // TopAccelerationBuffer tlas;
    std::unique_ptr<TopLevelAccelerationStructure> tlas;

    VmaAllocator allocator;
    std::shared_ptr<CommonFrameData> common_data;
    std::vector<std::unique_ptr<FrameData>> frame_data;
    uint32_t current_frame;

    std::unique_ptr<RTPipeline> pipeline;
    


    void setup_vulkan() {
        // Create Vulkan 1.3 instance  with validation layers
        // VULKAN_HPP_DEFAULT_DISPATCHER.init();

        vk::ApplicationInfo app_info("Vulkan Path Tracer", VK_MAKE_VERSION(1, 0, 0), nullptr, VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_3);

        const std::vector<const char*> validation_layers = {
            "VK_LAYER_KHRONOS_validation"
        };

        uint32_t extension_count = 0;
        const auto glfw_extensions = glfwGetRequiredInstanceExtensions(&extension_count);
        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + extension_count);
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        
        vk::InstanceCreateInfo create_info({}, &app_info, validation_layers.size(), validation_layers.data(), extensions.size(), extensions.data());

        try {
            instance = vk::createInstance(create_info);
            dl = vk::detail::DispatchLoaderDynamic(instance, vkGetInstanceProcAddr, device);

        } catch (const std::runtime_error& e) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }

        // Choose physical device which is discrete
        auto physical_devices = instance.enumeratePhysicalDevices();
        for (const auto& device : physical_devices) {
            if (device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                physical_device = device;
                break;
            }
        }

        if (!physical_device) {
            throw std::runtime_error("Failed to find a discrete GPU");
        }

        // Create surface
        auto glfw_window = window_system->get(window);
        glfwCreateWindowSurface(instance, glfw_window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&surface));
           

        // Create device with basic features, swapchain, and ray tracing enabled
        const std::vector<const char*> device_extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            // VK_NV_RAY_TRACING_VALIDATION_EXTENSION_NAME
        };
        float queue_priority = 1.0f;
        vk::DeviceQueueCreateInfo queue_create_info({}, 0, 1, &queue_priority);

        auto queue_family_properties = physical_device.getQueueFamilyProperties();
        graphics_queue_family_index = -1;
        present_queue_family_index = -1;

        for (size_t i = 0; i < queue_family_properties.size(); ++i) {
            if (queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                graphics_queue_family_index = static_cast<int>(i);
            }

            if (physical_device.getSurfaceSupportKHR(i, surface)) {
                present_queue_family_index = static_cast<int>(i);
            }

            if (graphics_queue_family_index != -1 && present_queue_family_index != -1) {
                break;
            }
        }

        if (graphics_queue_family_index == -1 || present_queue_family_index == -1) {
            throw std::runtime_error("Failed to find suitable queue families");
        }

        std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
        queue_create_infos.push_back(vk::DeviceQueueCreateInfo({}, graphics_queue_family_index, 1, &queue_priority));

        if (graphics_queue_family_index != present_queue_family_index) {
            queue_create_infos.push_back(vk::DeviceQueueCreateInfo({}, present_queue_family_index, 1, &queue_priority));
        }

        // Chain a bunch of physical device features

        // vk::PhysicalDeviceFeatures device_features = {};
        // device_features.samplerAnisotropy = VK_TRUE;
        // Ray tracing pipelines
        vk::PhysicalDeviceRayTracingPipelineFeaturesKHR physical_device_ray_tracing_pipeline_features{};
        physical_device_ray_tracing_pipeline_features.rayTracingPipeline = true;

        // Buffer device addresses (for acceleration structures)
        vk::PhysicalDeviceBufferDeviceAddressFeatures address_features;
        address_features.bufferDeviceAddress = true;
        address_features.pNext = &physical_device_ray_tracing_pipeline_features;
        
        // Acceleration structures
        vk::PhysicalDeviceAccelerationStructureFeaturesKHR acc_features;
        acc_features.accelerationStructure = true;
        acc_features.pNext = &address_features;

        // For bindless descriptors
        vk::PhysicalDeviceDescriptorIndexingFeatures indexing_features = {};
        indexing_features.shaderSampledImageArrayNonUniformIndexing = true;
        indexing_features.runtimeDescriptorArray = true;
        indexing_features.descriptorBindingVariableDescriptorCount = true;
        indexing_features.descriptorBindingPartiallyBound = true;
        indexing_features.pNext = &acc_features;



        // nv ray tracing validation
        vk::PhysicalDeviceRayTracingValidationFeaturesNV validation_features;
        validation_features.pNext = &indexing_features;


        vk::PhysicalDeviceFeatures2 device_features2;
        device_features2.pNext = &validation_features;

        vk::DeviceCreateInfo device_create_info(
            {},
            queue_create_infos,
            validation_layers,
            device_extensions,
            nullptr,
            &device_features2
        );

        // vk::DeviceCreateInfo device_create_info(
        //     {}, 
        //     queue_create_infos.size(),
        //     queue_create_infos.data(), 
        //     0, 
        //     nullptr, 
        //     device_extensions.size(),
        //     device_extensions.data(), 
        //     &device_features, 
        //     &device_features2
        //     );

        // vk::DeviceCreateInfo device_create_info({}, queue_create_infos.size(), queue_create_infos.data(), 0, nullptr, device_extensions.size(), device_extensions.data(), &device_features, &device_features2);

        device = physical_device.createDevice(device_create_info);

        // pool = Command_Pool(&device, &physical_device, vk::QueueFlagBits::eGraphics);
        // pool = Command_Pool((const vk::Device*)&device, (vk::PhysicalDevice)physical_device, vk::QueueFlagBits::eGraphics);
        // Create general command pool
        vk::CommandPoolCreateInfo pool_info{};
        pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        pool_info.queueFamilyIndex = graphics_queue_family_index; // assumes graphics and present queue family are the same
        general_command_pool = device.createCommandPool(pool_info);
        // Create VmaAllocator

        VmaAllocatorCreateInfo allocator_info = {};
        allocator_info.physicalDevice = physical_device;
        allocator_info.device = device;
        allocator_info.instance = instance;
        allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        vmaCreateAllocator(&allocator_info, &allocator);

    }

    void create_rt_pipeline() {

        // Create descriptor set bindings
        std::vector<vk::DescriptorSetLayoutBinding> bindings = {
            {0, vk::DescriptorType::eAccelerationStructureKHR, 1, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR},
            {1, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR},
            {2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR},
            {3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR}, // mesh data            
            {4, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR}, // instance data
            {5, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR}, // material data
            {6, vk::DescriptorType::eCombinedImageSampler, 128, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR}, // color texture data
            {7, vk::DescriptorType::eCombinedImageSampler, 128, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR}, // normal texture data
            {8, vk::DescriptorType::eCombinedImageSampler, 128, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR}, // metallic roughness texture data
            {9, vk::DescriptorType::eCombinedImageSampler, 128, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR}, // emissive texture data
        };

        // Create pipeline
        pipeline = std::make_unique<RTPipeline>(
            device, 
            allocator,
            dl,
            bindings,
            "shaders/pt_shader.rgen.spv", 
            "shaders/pt_shader.rmiss.spv", 
            "shaders/pt_shader.rchit.spv"
        );
        
    }

    // void create_descriptor_sets();
/*
    void create_pipeline() {
        // pipeline = Pipeline((VkDevice*)&device);
        pipeline = Pipeline((vk::Device*)&device, dl);
        pipeline.add_binding(vk::DescriptorType::eAccelerationStructureKHR);
        // pipeline.add_binding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        pipeline.add_binding(vk::DescriptorType::eStorageImage);
        pipeline.create_set();

        // create_descriptor_sets();

        std::string rengen_module_path = "shaders/shader.rgen.spv";
        std::string miss_module_path = "shaders/shader.rmiss.spv";
        std::string clhit_module_path = "shaders/shader.rchit.spv";

        pipeline.push_module(rengen_module_path, vk::ShaderStageFlagBits::eRaygenKHR);
        pipeline.push_module(miss_module_path, vk::ShaderStageFlagBits::eMissKHR);
        pipeline.push_module(clhit_module_path, vk::ShaderStageFlagBits::eClosestHitKHR);
        pipeline.create_rt_pipeline(3);
    }
    */

    void cleanup_vulkan() {
        vmaDestroyBuffer(allocator, sbt.buffer, sbt.allocation);
        pipeline.reset();
        device.destroyCommandPool(general_command_pool);
        // pool.destroy_pool();
        vmaDestroyAllocator(allocator);
        device.destroy();
        instance.destroySurfaceKHR(surface);
        instance.destroy();
    }  

    void create_buffer(vk::Buffer& buffer, vk::DeviceMemory& memory, vk::DeviceSize size, vk::BufferUsageFlags usage, 
                                vk::MemoryPropertyFlags properties, vk::SharingMode mode = vk::SharingMode::eExclusive);
    
    void create_mesh_buffer(TopLevelAccelerationStructure* tlas, const Primitive* mesh);

    vk::DeviceAddress get_device_address(vk::Buffer buffer) {
        vk::BufferDeviceAddressInfo info{};
        info.buffer = buffer;
        return device.getBufferAddress(&info, dl);
    }
    
    // void create_ubo();

    // void create_device_buffer(vk::Buffer& buffer, vk::DeviceMemory& memory, const void* vertices, vk::DeviceSize size, vk::BufferUsageFlags usage);

    std::pair<vk::Buffer, VmaAllocation> create_staging_buffer_with_data(const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage) {
        vk::Buffer staging_buffer;
        VmaAllocation staging_allocation;

        // Create a staging buffer for the data
        vk::BufferCreateInfo buffer_info{};
        buffer_info.size = size;
        buffer_info.usage = usage | vk::BufferUsageFlagBits::eTransferSrc;
        buffer_info.sharingMode = vk::SharingMode::eExclusive;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        // alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        // alloc_info.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        // alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        if (vmaCreateBuffer(allocator, reinterpret_cast<VkBufferCreateInfo*>(&buffer_info), &alloc_info, 
            reinterpret_cast<VkBuffer*>(&staging_buffer), &staging_allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create staging buffer");
        }

        // Map the buffer and copy the data
        if (data != nullptr) {
            void* mapped_data;
            vmaMapMemory(allocator, staging_allocation, &mapped_data);
            memcpy(mapped_data, data, static_cast<size_t>(size));
            vmaFlushAllocation(allocator, staging_allocation, 0, size);
            vmaUnmapMemory(allocator, staging_allocation);
        }
        return {staging_buffer, staging_allocation};
    }

    std::pair<vk::Buffer, VmaAllocation> create_device_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage) {
        vk::Buffer buffer;
        VmaAllocation allocation;
        vk::BufferCreateInfo buffer_info;
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = vk::SharingMode::eExclusive;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        
        if (vmaCreateBuffer(allocator, reinterpret_cast<VkBufferCreateInfo*>(&buffer_info), &alloc_info, 
            reinterpret_cast<VkBuffer*>(&buffer), &allocation, nullptr) != VK_SUCCESS) {
            std::cout << "Failed to create device buffer" << std::endl;
            throw std::runtime_error("Failed to create device buffer");
        }

        return {buffer, allocation};
    }

    std::pair<vk::Buffer, VmaAllocation> create_device_buffer_with_data(const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage) {
        auto [staging_buffer, staging_allocation] = create_staging_buffer_with_data(data, size, usage);
        // std::cout << "Successfully created staging buffer" << std::endl;

        // Create a device-local buffer for the data
        auto [buffer, allocation] = create_device_buffer(size,  usage | vk::BufferUsageFlagBits::eTransferDst);
 
        // Copy the data from the staging buffer to the device-local buffer
        // auto & command_pool = common_data->get_command_pool();
        auto cmd_buffer = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(general_command_pool, vk::CommandBufferLevel::ePrimary, 1)).front();
        vk::BufferCopy copy_region;
        copy_region.size = size;
        auto begin_info = vk::CommandBufferBeginInfo();
        begin_info.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

        cmd_buffer.begin(begin_info);
        cmd_buffer.copyBuffer(staging_buffer, buffer, 1, &copy_region);
        cmd_buffer.end();
        auto q = device.getQueue(graphics_queue_family_index, 0);
        vk::SubmitInfo submit_info;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd_buffer;
        q.submit(1, &submit_info, nullptr);
        q.waitIdle();
        device.freeCommandBuffers(general_command_pool, 1, &cmd_buffer);

        vmaDestroyBuffer(allocator, staging_buffer, staging_allocation);

        return {buffer, allocation};
    }

    void create_BLAS(TopLevelAccelerationStructure * tlas, const MeshBuffer* mesh);

    void create_TLAS(TopLevelAccelerationStructure * tlas);

    vk::Format get_vk_format(TextureMap::TextureType format) {
        switch (format) {
            case TextureMap::TextureType::baseColorTexture:
                return vk::Format::eR8G8B8A8Srgb;
            case TextureMap::TextureType::normalTexture:
                return vk::Format::eR8G8B8A8Unorm;
            case TextureMap::TextureType::metallicRoughnessTexture:
                return vk::Format::eR8G8B8A8Unorm;
            case TextureMap::TextureType::emissiveTexture:
                return vk::Format::eR8G8B8A8Srgb;
            default:
                throw std::runtime_error("Unsupported texture format");
        }
    }

    void verify_format(TextureMap& map, vk::Format format) {
        if (map.type() == TextureMap::TextureType::baseColorTexture) {
            if (map.channels() != 4) {
                throw std::runtime_error("Base color texture must have 4 channels");
            }
        } else if (map.type() == TextureMap::TextureType::normalTexture) {
            if (map.channels() != 4) {
                throw std::runtime_error("Normal texture must have 4 channels");
            }
        } else if (map.type() == TextureMap::TextureType::metallicRoughnessTexture) {
            if (map.channels() != 4) {
                throw std::runtime_error("Metallic roughness texture must have 4 channels");
            }
        } else if (map.type() == TextureMap::TextureType::emissiveTexture) {
            if (map.channels() != 4) {
                throw std::runtime_error("Emissive texture must have 4 channel");
            }
        }
    }

    ImageStorage::Textures create_image(TextureMap& uvmap) {

        auto texture_format = get_vk_format(uvmap.type());
        verify_format(uvmap, texture_format);

        VkDeviceSize size = uvmap.width() * uvmap.height() * uvmap.channels();

        auto [staging_buffer, staging_allocation] = create_staging_buffer_with_data(uvmap.data(), size, vk::BufferUsageFlagBits::eTransferSrc);
        ImageStorage::Textures current_memory;
        
        vk::ImageCreateInfo create_info = images.get_create_info(uvmap.width(), uvmap.height(), texture_format);
        create_info.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(allocator, reinterpret_cast<const VkImageCreateInfo*>(&create_info), 
                    &allocInfo, reinterpret_cast<VkImage*>(&current_memory.image), &current_memory.memory, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image with VMA!");
        }

        vk::CommandBuffer command_buffer = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(
                general_command_pool, vk::CommandBufferLevel::ePrimary, 1)).front();
        
        command_buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        
        // Transition texture image to transfer destination optimal
        vk::ImageMemoryBarrier pre_barrier;
        pre_barrier.oldLayout = vk::ImageLayout::eUndefined;
        pre_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
        pre_barrier.image = current_memory.image;
        pre_barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        pre_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        pre_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eAllCommands, 
            vk::PipelineStageFlagBits::eAllCommands,
            {}, {}, {}, {pre_barrier} 
        );

        vk::BufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0; 
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = vk::Offset3D{0, 0, 0};
        region.imageExtent = vk::Extent3D{static_cast<uint32_t>(uvmap.width()), static_cast<uint32_t>(uvmap.height()), 1};

        std::vector<vk::BufferImageCopy> regions = {region};
        command_buffer.copyBufferToImage(staging_buffer, current_memory.image, vk::ImageLayout::eTransferDstOptimal, regions.size(), regions.data());
        
        vk::ImageMemoryBarrier pos_barrier;
        pos_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        pos_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        pos_barrier.image = current_memory.image;
        pos_barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        pos_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        pos_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;;

        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eAllCommands, 
            vk::PipelineStageFlagBits::eAllCommands,
            {}, {}, {}, {pos_barrier} 
        );

        command_buffer.end();

        auto q = device.getQueue(graphics_queue_family_index, 0);
        vk::SubmitInfo submit_info;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;
        q.submit(1, &submit_info, nullptr);
        q.waitIdle();
        device.freeCommandBuffers(general_command_pool, 1, &command_buffer);
        
        vmaDestroyBuffer(allocator, staging_buffer, staging_allocation);

        // Create sampler
        vk::PhysicalDeviceProperties properties = physical_device.getProperties();
        vk::SamplerCreateInfo sampler_info{};
        sampler_info.sType = vk::StructureType::eSamplerCreateInfo;
		sampler_info.magFilter = vk::Filter::eLinear;
		sampler_info.minFilter = vk::Filter::eLinear;
		sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
		sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
		sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
		sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
		sampler_info.mipLodBias = 0.0f;
		sampler_info.compareOp = vk::CompareOp::eNever;
		sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		// sampler_info.anisotropyEnable = VK_TRUE;
		sampler_info.borderColor = vk::BorderColor::eFloatOpaqueBlack;

        current_memory.sampler = device.createSampler(sampler_info);
        
        // Create image view
		vk::ImageViewCreateInfo view_create_info;
		view_create_info.sType = vk::StructureType::eImageViewCreateInfo;
		view_create_info.viewType = vk::ImageViewType::e2D;
		view_create_info.format = texture_format; //vk::Format::eR8G8B8A8Unorm;
        view_create_info.image = current_memory.image;
		view_create_info.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        current_memory.view = device.createImageView(view_create_info);

        // images.images_memory.push_back(current_memory);
        return current_memory;
    }



    void create_textures() {
        // TextureMap uvmap;
        uint32_t n_material = 0;
        for (; n_material < scene->material_size(); n_material++) {
            for (auto& texture : scene->material(n_material)) {
                // uvmap = texture;
                if (texture.type() == TextureMap::TextureType::baseColorTexture) {
                    images.base_color_textures.push_back(create_image(texture));
                }
                else if (texture.type() == TextureMap::TextureType::normalTexture) {
                    images.normal_textures.push_back(create_image(texture));
                } else if (texture.type() == TextureMap::TextureType::metallicRoughnessTexture) {
                    images.metallic_roughness_textures.push_back(create_image(texture));
                } else if (texture.type() == TextureMap::TextureType::emissiveTexture) {
                    images.emissive_textures.push_back(create_image(texture));
                } 
                else {
                    std::cout << "Unknown texture type" << std::endl;
                }
            }
            tlas->material_data.push_back(MaterialData{(float)scene->material(n_material).get_transmission()});
        }

        // copy material data to device buffer
        auto [mat_buf, mat_alloc] = create_device_buffer_with_data(tlas->material_data.data(), sizeof(MaterialData) * tlas->material_data.size(), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst);
        tlas->material_data_buffer = mat_buf;
        tlas->material_data_allocation = mat_alloc;

        std::cout << "Created " << images.base_color_textures.size() << " base color textures" << std::endl;
        std::cout << "Created " << images.normal_textures.size() << " normal textures" << std::endl;
        std::cout << "Created " << images.metallic_roughness_textures.size() << " metallic textures" << std::endl;
        std::cout << "Created " << images.emissive_textures.size() << " emissive textures" << std::endl;        
    }

    public:

    Renderer(WindowHandle window, WindowSystemGLFW * window_system): window(window), window_system(window_system) {

        graphics_queue_family_index = -1;
        present_queue_family_index = -1;
        setup_vulkan();

        swapchain = std::make_unique<Swapchain>(physical_device, device, window_system->get(window), surface);
        
        // create_pipeline();
        create_rt_pipeline();
        create_sbt();

        // load_scene("glTF-Sample-Assets/Models/Lantern/glTF/Lantern.gltf");
        // load_scene("glTF-Sample-Assets/Models/FlightHelmet/glTF/FlightHelmet.gltf");
        // load_scene("glTF-Sample-Assets/Models/DamagedHelmet/glTF/DamagedHelmet.gltf");
        // load_scene("glTF-Sample-Assets/Models/AntiqueCamera/glTF/AntiqueCamera.gltf");
        // load_scene("glTF-Sample-Assets/Models/Duck/glTF/Duck.gltf");
        load_scene("glTF-Sample-Assets/Models/ABeautifulGame/glTF/ABeautifulGame.gltf");
        // load_scene("glTF-Sample-Assets/Models/CarConcept/glTF/CarConcept.gltf");
        // load_scene("glTF-Sample-Assets/Models/Cube/glTF/Cube.gltf");

        frame_setup();

        std::cout << "Renderer created" << std::endl;
    }

    ~Renderer() {
        frame_cleanup();
        if (swapchain) {
            swapchain.reset();
        }
        tlas.reset();
        scene.reset();
        for (auto& image : images.base_color_textures) {
            device.destroyImageView(image.view);
            device.destroySampler(image.sampler);
            vmaDestroyImage(allocator, image.image, image.memory);
        }
        for (auto & image: images.normal_textures) {
            device.destroyImageView(image.view);
            device.destroySampler(image.sampler);
            vmaDestroyImage(allocator, image.image, image.memory);
        }
        for (auto & image: images.metallic_roughness_textures) {
            device.destroyImageView(image.view);
            device.destroySampler(image.sampler);
            vmaDestroyImage(allocator, image.image, image.memory);
        }
        for (auto & image: images.emissive_textures) {
            device.destroyImageView(image.view);
            device.destroySampler(image.sampler);
            vmaDestroyImage(allocator, image.image, image.memory);
        }
        cleanup_vulkan();
        std::cout << "Renderer destroyed" << std::endl;
    }

    
    void load_scene(std::string file_path);

    void create_sbt();

    // Set up common and frame-specific data
    void frame_setup() {

        // You'll need to recreate all this if the swapchain changes
        common_data = std::make_shared<CommonFrameData>(device, allocator, swapchain->get_num_images(), graphics_queue_family_index);
        current_frame = 0;
        // frame_data.resize(swapchain->get_num_images());
        for (int i = 0; i < swapchain->get_num_images(); i++) {
            frame_data.emplace_back(std::make_unique<FrameData>(common_data, r_width, r_height, i));
        }


        // Create descriptor sets for the pipeline

        for (int i = 0; i < swapchain->get_num_images(); i++) {

            vk::DescriptorSetAllocateInfo alloc_info{};
            alloc_info.descriptorPool = frame_data[i]->descriptor_pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &pipeline->descriptor_set_layout;

            vk::DescriptorSet descriptor_set;
            device.allocateDescriptorSets(&alloc_info, &descriptor_set);

            frame_data[i]->descriptor_sets[pipeline->descriptor_set_layout] = descriptor_set;

            // Fill descriptor set

            // Acceleration Structure
            vk::WriteDescriptorSet acc_desc_write;
            acc_desc_write.dstSet = descriptor_set;
            acc_desc_write.dstBinding = 0;
            acc_desc_write.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
            acc_desc_write.descriptorCount = 1;
            vk::WriteDescriptorSetAccelerationStructureKHR  acc_list;
            acc_list.accelerationStructureCount = 1;
            acc_list.pAccelerationStructures = &tlas->structure;
            acc_desc_write.pNext = &acc_list;


            // RT image descriptor  
            vk::WriteDescriptorSet img_desc_write;
            img_desc_write.dstSet = descriptor_set;
            img_desc_write.dstBinding = 1;
            img_desc_write.descriptorType = vk::DescriptorType::eStorageImage;
            img_desc_write.descriptorCount = 1;

            vk::DescriptorImageInfo img_info;
            img_info.imageLayout = vk::ImageLayout::eGeneral;
            img_info.imageView = frame_data[i]->rt_image_view;
            img_desc_write.pImageInfo = &img_info;

            // Camera UBO descriptor
            vk::WriteDescriptorSet cam_desc_write;
            cam_desc_write.dstSet = descriptor_set;
            cam_desc_write.dstBinding = 2;
            cam_desc_write.descriptorType = vk::DescriptorType::eUniformBuffer;
            cam_desc_write.descriptorCount = 1;

            vk::DescriptorBufferInfo cb_info;
            cb_info.buffer = frame_data[i]->camera_buffer;
            cb_info.offset = 0;
            cb_info.range = sizeof(RTCamera);

            cam_desc_write.pBufferInfo = &cb_info;

            // Mesh data descriptor
            vk::WriteDescriptorSet mesh_desc_write;
            mesh_desc_write.dstSet = descriptor_set;
            mesh_desc_write.dstBinding = 3;
            mesh_desc_write.descriptorType = vk::DescriptorType::eStorageBuffer;
            mesh_desc_write.descriptorCount = 1;
            vk::DescriptorBufferInfo mesh_info;
            mesh_info.buffer = tlas->mesh_data_buffer;
            mesh_info.offset = 0;
            mesh_info.range = sizeof(MeshData) * tlas->mesh_data.size();
            mesh_desc_write.pBufferInfo = &mesh_info;

            // Instance data descriptor
            vk::WriteDescriptorSet instance_desc_write;
            instance_desc_write.dstSet = descriptor_set;
            instance_desc_write.dstBinding = 4;
            instance_desc_write.descriptorType = vk::DescriptorType::eStorageBuffer;
            instance_desc_write.descriptorCount = 1;
            vk::DescriptorBufferInfo instance_info;
            instance_info.buffer = tlas->instance_data_buffer;
            instance_info.offset = 0;
            instance_info.range = sizeof(InstanceData) * tlas->instance_data.size();
            instance_desc_write.pBufferInfo = &instance_info;

            // Instance data descriptor
            vk::WriteDescriptorSet material_desc_write;
            material_desc_write.dstSet = descriptor_set;
            material_desc_write.dstBinding = 5;
            material_desc_write.descriptorType = vk::DescriptorType::eStorageBuffer;
            material_desc_write.descriptorCount = 1;
            vk::DescriptorBufferInfo mat_info;
            mat_info.buffer = tlas->material_data_buffer;
            mat_info.offset = 0;
            mat_info.range = sizeof(MaterialData) * tlas->material_data.size();
            material_desc_write.pBufferInfo = &mat_info;

            // Base color texture descriptor
            vk::WriteDescriptorSet texture_desc_write;
            texture_desc_write.dstSet = descriptor_set;
            texture_desc_write.dstBinding = 6;
            texture_desc_write.descriptorType = vk::DescriptorType::eCombinedImageSampler;

            std::vector<vk::DescriptorImageInfo> tex_infos;
            for (auto & image : images.base_color_textures) {
                tex_infos.push_back(vk::DescriptorImageInfo(image.sampler, image.view, vk::ImageLayout::eShaderReadOnlyOptimal));
            }
            
            texture_desc_write.pImageInfo = tex_infos.data();
            texture_desc_write.descriptorCount = static_cast<uint32_t>(tex_infos.size());

            // Normal texture descriptor
            vk::WriteDescriptorSet normal_desc_write;
            normal_desc_write.dstSet = descriptor_set;
            normal_desc_write.dstBinding = 7;
            normal_desc_write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            std::vector<vk::DescriptorImageInfo> normal_tex_infos;
            for (auto & image : images.normal_textures) {
                normal_tex_infos.push_back(vk::DescriptorImageInfo(image.sampler, image.view, vk::ImageLayout::eShaderReadOnlyOptimal));
            }
            normal_desc_write.pImageInfo = normal_tex_infos.data();
            normal_desc_write.descriptorCount = static_cast<uint32_t>(images.normal_textures.size());

            // Metallic roughness texture descriptor
            vk::WriteDescriptorSet metallic_desc_write;
            metallic_desc_write.dstSet = descriptor_set;
            metallic_desc_write.dstBinding = 8;
            metallic_desc_write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            std::vector<vk::DescriptorImageInfo> metallic_tex_infos;
            for (auto & image : images.metallic_roughness_textures) {
                metallic_tex_infos.push_back(vk::DescriptorImageInfo(image.sampler, image.view, vk::ImageLayout::eShaderReadOnlyOptimal));
            }
            metallic_desc_write.pImageInfo = metallic_tex_infos.data();
            metallic_desc_write.descriptorCount = static_cast<uint32_t>(images.metallic_roughness_textures.size());

            // Emissive texture descriptor
            vk::WriteDescriptorSet emissive_desc_write;
            emissive_desc_write.dstSet = descriptor_set;
            emissive_desc_write.dstBinding = 9;
            emissive_desc_write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            std::vector<vk::DescriptorImageInfo> emissive_tex_infos;
            for (auto & image : images.emissive_textures) {
                emissive_tex_infos.push_back(vk::DescriptorImageInfo(image.sampler, image.view, vk::ImageLayout::eShaderReadOnlyOptimal));
            }
            emissive_desc_write.pImageInfo = emissive_tex_infos.data();
            emissive_desc_write.descriptorCount = static_cast<uint32_t>(images.emissive_textures.size());



            // Update descriptor set
            vk::WriteDescriptorSet writes[] = {
                acc_desc_write, 
                img_desc_write, 
                cam_desc_write,
                mesh_desc_write,
                instance_desc_write,
                material_desc_write,
                texture_desc_write,
                normal_desc_write,
                metallic_desc_write,
                emissive_desc_write
            };
            device.updateDescriptorSets(10, writes, 0, nullptr);
        }


        // Create empty acceleration structure

    }

    void frame_cleanup() {

        for (auto& frame : frame_data) {
            device.waitForFences(1, &frame->fence, VK_TRUE, UINT64_MAX);
        }
        auto q = device.getQueue(graphics_queue_family_index, 0);
        q.waitIdle();

        device.waitIdle();
        frame_data.clear();
        common_data.reset();
    }
    

    void render(const FrameConstants & frame_constants) {

        // set up work for the current frame
        // std::cout << "Waiting for command buffer" << std::endl;
        device.waitForFences(1, &frame_data[current_frame]->fence, VK_TRUE, UINT64_MAX);
        device.resetFences(1, &frame_data[current_frame]->fence);

        // acquire next swapchain image
        uint32_t swapchain_image_index;
        auto ret_acquire = device.acquireNextImageKHR(swapchain->get_swapchain(), UINT64_MAX, frame_data[current_frame]->sc_image_available, nullptr, &swapchain_image_index);
        if (ret_acquire == vk::Result::eErrorOutOfDateKHR) {
            // TODO: recreate swapchain
            return;
        } else if (ret_acquire != vk::Result::eSuccess && ret_acquire != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("Failed to acquire swapchain image");
        }

        // Start recording command buffer
        auto & cmd_buffer = frame_data[current_frame]->command_buffer;
        cmd_buffer.reset(vk::CommandBufferResetFlags());
        vk::CommandBufferBeginInfo begin_info{};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
        cmd_buffer.begin(begin_info);

        // std::cout << "Recorded command buffer" << std::endl;

        // Clear rt_image with red (for testing)
        vk::ClearColorValue clear_color(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
        vk::ImageSubresourceRange subresource_range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        vk::ImageMemoryBarrier barrier(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, frame_data[current_frame]->rt_image, subresource_range);
        cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr, nullptr, barrier);
        cmd_buffer.clearColorImage(frame_data[current_frame]->rt_image, vk::ImageLayout::eTransferDstOptimal, clear_color, subresource_range);
        barrier = vk::ImageMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, frame_data[current_frame]->rt_image, subresource_range);
        cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eRayTracingShaderKHR, vk::DependencyFlags(), nullptr, nullptr, barrier);
        
        // Update camera buffer (parameters updated externally)
        void* mapped_data = nullptr;
        vmaMapMemory(allocator, 
            frame_data[current_frame]->staging_buffer_allocation, &mapped_data);
        std::memcpy(mapped_data, &camera, sizeof(RTCamera));
        vmaUnmapMemory(allocator, frame_data[current_frame]->staging_buffer_allocation);
    
        vk::BufferCopy copy_region{};
        copy_region.srcOffset = 0;
        copy_region.dstOffset = 0;
        copy_region.size = sizeof(RTCamera);
    
        cmd_buffer.copyBuffer(frame_data[current_frame]->staging_buffer, frame_data[current_frame]->camera_buffer, copy_region);
        // Add a pipeline barrier to ensure the copy operation is complete before ray tracing
        vk::BufferMemoryBarrier buffer_barrier(
            vk::AccessFlagBits::eTransferWrite, 
            vk::AccessFlagBits::eShaderRead, 
            VK_QUEUE_FAMILY_IGNORED, 
            VK_QUEUE_FAMILY_IGNORED, 
            frame_data[current_frame]->camera_buffer, 
            0, 
            sizeof(RTCamera)
        );
        cmd_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, 
            vk::PipelineStageFlagBits::eRayTracingShaderKHR, 
            vk::DependencyFlags(), 
            nullptr, 
            buffer_barrier, 
            nullptr
        );

        // Ray tracing commands ....
        cmd_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, pipeline->pipeline);
        cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR,
                                      pipeline->layout, 0,
                                      frame_data[current_frame]->descriptor_sets[pipeline->descriptor_set_layout],
                                      nullptr);

        cmd_buffer.traceRaysKHR(
            &sbt.raygen_region, 
            &sbt.miss_region,
            &sbt.hit_region,
            &sbt.callable_region,
            r_width, r_height, 1,
            dl
        );


        // After ray tracing is done, transition the image to a transfer source layout
        barrier = vk::ImageMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, frame_data[current_frame]->rt_image, subresource_range);
        cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eRayTracingShaderKHR, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr, nullptr, barrier);
        

        // Blit rt_image into swapchain image
        vk::ImageBlit blit(
            vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            { vk::Offset3D{0, 0, 0}, vk::Offset3D{r_width, r_height, 1} },
            vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            { vk::Offset3D{0, 0, 0}, vk::Offset3D{r_width, r_height, 1} }
        );
        vk::ImageSubresourceRange subresource_range_dst(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        vk::ImageMemoryBarrier barrier_dst(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapchain->get_image(swapchain_image_index), subresource_range_dst);
        cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr, nullptr, barrier_dst);
        cmd_buffer.blitImage(frame_data[current_frame]->rt_image, vk::ImageLayout::eTransferSrcOptimal, swapchain->get_image(swapchain_image_index), vk::ImageLayout::eTransferDstOptimal, 1, &blit, vk::Filter::eNearest);
        barrier_dst = vk::ImageMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapchain->get_image(swapchain_image_index), subresource_range_dst);
        // Transition swapchain image to present layout
        cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlags(), nullptr, nullptr, barrier_dst);

        // End command buffer  
        cmd_buffer.end();

        // submit to queue
        vk::SubmitInfo submit_info{};
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd_buffer;
        submit_info.waitSemaphoreCount = 1;
        const vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eTransfer;
        submit_info.pWaitDstStageMask = &wait_stage;
        submit_info.pWaitSemaphores = &frame_data[current_frame]->sc_image_available;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &frame_data[current_frame]->sem;
        auto queue = device.getQueue(graphics_queue_family_index, 0);
        queue.submit(1, &submit_info, frame_data[current_frame]->fence);
        // std::cout << "Submitted command buffer" << std::endl;

        // Prepare for present
        vk::PresentInfoKHR present_info{};
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &frame_data[current_frame]->sem;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain->get_swapchain();
        present_info.pImageIndices = &swapchain_image_index;
        queue.presentKHR(present_info);

        current_frame  = (current_frame + 1) % swapchain->get_num_images();

    }

    RTCamera& get_camera() {
        return camera;
    }
};