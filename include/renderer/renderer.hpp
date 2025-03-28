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

#include <unordered_map>
#include <memory>

// std::string rengen_module_path = "";
// std::string miss_module_path = "";
// std::string clhit_module_path = "";

struct ShaderBindingTable {
    vk::Buffer buffer;
    vk::DeviceMemory memory;
};

struct StagingBuffer {
    void* data;
    
    vk::Buffer buffer;
    vk::DeviceMemory memory;
};

struct Camera {
    glm::mat4 view;
    glm::mat4 projection;
};

struct UnifromBuffer {
    Camera matrices;

    vk::Buffer buffer;
    vk::DeviceMemory memory;

    StagingBuffer map;
};

struct MeshBuffer {
    vk::Buffer vertex_buffer;
    vk::DeviceMemory vertex_memory;

    vk::Buffer index_buffer;
    vk::DeviceMemory index_memory;
    
    uint32_t size;
};

struct InstanceBuffer {
    MeshBuffer* mesh_buffer;
    glm::mat4 transformation;
};

struct AccelerationBuffer {
    vk::AccelerationStructureKHR as;

    vk::Buffer buffer;
    vk::DeviceMemory memory;
    vk::DeviceAddress as_addr;
};

struct TopAccelerationBuffer {
    AccelerationBuffer as;

    // needs a separated buffer for instances
    vk::Buffer buffer;
    vk::DeviceMemory memory;
};

class Renderer {
    private:
    // hard code the dimensions for now
    static constexpr int r_width = 800;
    static constexpr int r_height = 600;
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

    std::unordered_map<const Mesh*, MeshBuffer> meshes{};
    std::vector<InstanceBuffer> objects{};

    Command_Pool pool;
    // Pipeline pipeline;

    Scene scene;

    UnifromBuffer camera;

    // ShaderBindingTable rgen_sbt;
    // ShaderBindingTable miss_sbt;
    // ShaderBindingTable hit_sbt;

    // std::vector<ShaderBindingTable> SBTs;

    std::unordered_map<const MeshBuffer*, AccelerationBuffer> blas;
    TopAccelerationBuffer tlas;

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
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
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

        vk::PhysicalDeviceFeatures2 device_features2;
        device_features2.pNext = &acc_features;

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

        pool = Command_Pool(&device, &physical_device, vk::QueueFlagBits::eGraphics);
        // pool = Command_Pool((const vk::Device*)&device, (vk::PhysicalDevice)physical_device, vk::QueueFlagBits::eGraphics);

        // Create VmaAllocator

        VmaAllocatorCreateInfo allocator_info = {};
        allocator_info.physicalDevice = physical_device;
        allocator_info.device = device;
        allocator_info.instance = instance;
        allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        vmaCreateAllocator(&allocator_info, &allocator);

        // create_pipeline();
        create_rt_pipeline();

    }

    void create_rt_pipeline() {

        // Create descriptor set bindings
        std::vector<vk::DescriptorSetLayoutBinding> bindings = {
            {0, vk::DescriptorType::eAccelerationStructureKHR, 1, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR},
            {1, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR}
        };

        // Create pipeline
        pipeline = std::make_unique<RTPipeline>(
            device, 
            dl,
            bindings,
            "shaders/shader.rgen.spv", 
            "shaders/shader.rmiss.spv", 
            "shaders/shader.rchit.spv"
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
        pipeline.reset();
        vmaDestroyAllocator(allocator);
        device.destroy();
        instance.destroySurfaceKHR(surface);
        instance.destroy();
    }  

    void create_buffer(vk::Buffer& buffer, vk::DeviceMemory& memory, vk::DeviceSize size, vk::BufferUsageFlags usage, 
                                vk::MemoryPropertyFlags properties, vk::SharingMode mode = vk::SharingMode::eExclusive);
    
    void create_mesh_buffer(const Mesh* mesh);

    vk::DeviceAddress get_device_address(vk::Buffer buffer) {
        vk::BufferDeviceAddressInfo info{};
        info.buffer = buffer;
        return device.getBufferAddress(&info);
    }
    
    void create_ubo();

    void create_device_buffer(vk::Buffer& buffer, vk::DeviceMemory& memory, const void* vertices, vk::DeviceSize size, vk::BufferUsageFlags usage);

    void create_BLAS(const MeshBuffer* mesh);

    void create_TLAS();

    public:

    Renderer(WindowHandle window, WindowSystemGLFW * window_system): window(window), window_system(window_system) {

        graphics_queue_family_index = -1;
        present_queue_family_index = -1;
        setup_vulkan();

        swapchain = std::make_unique<Swapchain>(physical_device, device, window_system->get(window), surface);
        
        frame_setup();

        std::cout << "Renderer created" << std::endl;
    }

    ~Renderer() {
        frame_cleanup();
        if (swapchain) {
            swapchain.reset();
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

        // Scene specific stuff?
        // Create an empty acceleration structure with buffer using VMA
        vk::BufferCreateInfo buffer_info{};
        buffer_info.size = 1024; // Placeholder
        buffer_info.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        buffer_info.sharingMode = vk::SharingMode::eExclusive;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateBuffer(allocator, reinterpret_cast<VkBufferCreateInfo*>(&buffer_info), &alloc_info, 
        reinterpret_cast<VkBuffer*>(&common_data->tlas_buffer), &common_data->tlas_allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create TLAS buffer");
        }

        // Create empty TLAS
        vk::AccelerationStructureCreateInfoKHR acc_create_info{};
        acc_create_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;
        acc_create_info.size = 0; // No geometry yet
        acc_create_info.buffer = common_data->tlas_buffer;

        common_data->tlas = device.createAccelerationStructureKHR(acc_create_info, nullptr, dl);
        
        // Create descriptor sets for the pipeline
        vk::DescriptorSetLayoutBinding bindings[2] = {};

        // Binding 0: Acceleration Structure
        bindings[0].binding = 0;
        bindings[0].descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR;
        
        // Binding 1: Storage Image
        bindings[1].binding = 1;
        bindings[1].descriptorType = vk::DescriptorType::eStorageImage;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR;
        
        vk::DescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = bindings;

        vk::DescriptorSetLayout descriptor_set_layout;

        device.createDescriptorSetLayout(&layoutInfo, nullptr, &descriptor_set_layout);

        for (int i = 0; i < swapchain->get_num_images(); i++) {

            vk::DescriptorSetAllocateInfo alloc_info{};
            alloc_info.descriptorPool = frame_data[i]->descriptor_pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &descriptor_set_layout;

            vk::DescriptorSet descriptor_set;
            device.allocateDescriptorSets(&alloc_info, &descriptor_set);

            frame_data[i]->descriptor_sets[descriptor_set_layout] = descriptor_set;

            // Fill descriptor set
            vk::WriteDescriptorSet acc_desc_write;
            acc_desc_write.dstSet = descriptor_set;
            acc_desc_write.dstBinding = 0;
            acc_desc_write.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
            acc_desc_write.descriptorCount = 1;
            vk::WriteDescriptorSetAccelerationStructureKHR  acc_list;
            acc_list.accelerationStructureCount = 1;
            acc_list.pAccelerationStructures = &common_data->tlas;
            acc_desc_write.pNext = &acc_list;

            vk::WriteDescriptorSet img_desc_write;
            img_desc_write.dstSet = descriptor_set;
            img_desc_write.dstBinding = 1;
            img_desc_write.descriptorType = vk::DescriptorType::eStorageImage;
            img_desc_write.descriptorCount = 1;

            vk::DescriptorImageInfo img_info;
            img_info.imageLayout = vk::ImageLayout::eGeneral;
            img_info.imageView = frame_data[i]->rt_image_view;
            img_desc_write.pImageInfo = &img_info;

            vk::WriteDescriptorSet writes[] = {acc_desc_write, img_desc_write};
            device.updateDescriptorSets(2, writes, 0, nullptr);



        }


        // Create empty acceleration structure

    }

    void frame_cleanup() {

        for (auto& frame : frame_data) {
            device.waitForFences(1, &frame->fence, VK_TRUE, UINT64_MAX);
        }
        auto q = device.getQueue(graphics_queue_family_index, 0);
        q.waitIdle();

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
        vk::CommandBufferBeginInfo begin_info{};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
        cmd_buffer.begin(begin_info);

        // std::cout << "Recorded command buffer" << std::endl;

        // Clear rt_image with red (for testing)
        vk::ClearColorValue clear_color(std::array<float, 4>{1.0f, 0.0f, 0.0f, 1.0f});
        vk::ImageSubresourceRange subresource_range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        vk::ImageMemoryBarrier barrier(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, frame_data[current_frame]->rt_image, subresource_range);
        cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr, nullptr, barrier);
        cmd_buffer.clearColorImage(frame_data[current_frame]->rt_image, vk::ImageLayout::eTransferDstOptimal, clear_color, subresource_range);
        barrier = vk::ImageMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, frame_data[current_frame]->rt_image, subresource_range);
        cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eRayTracingShaderKHR, vk::DependencyFlags(), nullptr, nullptr, barrier);
        
        // Ray tracing commands ....
        // cmd_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, pipeline.get_pipeline());
        // cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, pipeline.get_layout(), 0, 
        // frame_data[current_frame]->descriptor_set, nullptr);

        // cmd_buffer.traceRaysKHR(
        //     rgen_sbt, 
        //     miss_sbt.buffer,
        //     hit_sbt.buffer,
        //     nullptr,
        //     r_width, r_height, 1
        // );


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
};