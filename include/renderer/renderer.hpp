#pragma once
#include <renderer/vulkan.hpp>

#include <renderer/swapchain.hpp>
#include <renderer/command_pool.hpp>
#include <renderer/pipeline.hpp>
#include <renderer/window/window_system_glfw.hpp>

#include <geometry/geometry.hpp>
#include <renderer/frame_constants.hpp>
#include <renderer/frame_data.hpp>

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
    Pipeline pipeline;

    Scene scene;

    UnifromBuffer camera;

    ShaderBindingTable rgen_sbt;
    ShaderBindingTable miss_sbt;
    ShaderBindingTable hit_sbt;

    // std::vector<ShaderBindingTable> SBTs;

    std::unordered_map<const MeshBuffer*, AccelerationBuffer> blas;
    TopAccelerationBuffer tlas;

    VmaAllocator allocator;
    std::shared_ptr<CommonFrameData> common_data;
    std::vector<std::unique_ptr<FrameData>> frame_data;
    uint32_t current_frame;
    


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
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME
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

        vk::PhysicalDeviceFeatures device_features = {};
        device_features.samplerAnisotropy = VK_TRUE;

        vk::PhysicalDeviceRayTracingPipelineFeaturesKHR physical_device_ray_tracing_pipeline_features{};
        physical_device_ray_tracing_pipeline_features.rayTracingPipeline = true;

        vk::DeviceCreateInfo device_create_info({}, queue_create_infos.size(), queue_create_infos.data(), 0, nullptr, device_extensions.size(), device_extensions.data(), &device_features, &physical_device_ray_tracing_pipeline_features);

        device = physical_device.createDevice(device_create_info);

        pool = Command_Pool(&device, &physical_device, vk::QueueFlagBits::eGraphics);
        // pool = Command_Pool((const vk::Device*)&device, (vk::PhysicalDevice)physical_device, vk::QueueFlagBits::eGraphics);
        // create_pipeline();

        // Create VmaAllocator

        VmaAllocatorCreateInfo allocator_info = {};
        allocator_info.physicalDevice = physical_device;
        allocator_info.device = device;
        allocator_info.instance = instance;
        vmaCreateAllocator(&allocator_info, &allocator);
    }

    void create_descriptor_sets();

    void create_pipeline() {
        // pipeline = Pipeline((VkDevice*)&device);
        pipeline = Pipeline((vk::Device*)&device, dl);
        pipeline.add_binding(vk::DescriptorType::eAccelerationStructureKHR);
        // pipeline.add_binding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        pipeline.add_binding(vk::DescriptorType::eUniformBuffer);
        pipeline.create_set();

        create_descriptor_sets();

        std::string rengen_module_path = "shaders/shader.rgen.spv";
        std::string miss_module_path = "shaders/shader.rmiss.spv";
        std::string clhit_module_path = "shaders/shader.rchit.spv";

        pipeline.push_module(rengen_module_path, vk::ShaderStageFlagBits::eRaygenKHR);
        pipeline.push_module(miss_module_path, vk::ShaderStageFlagBits::eMissKHR);
        pipeline.push_module(clhit_module_path, vk::ShaderStageFlagBits::eClosestHitKHR);
        pipeline.create_rt_pipeline(3);
    }

    void cleanup_vulkan() {
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

        auto & cmd_buffer = frame_data[current_frame]->command_buffer;
        device.waitForFences(1, &frame_data[current_frame]->fence, VK_TRUE, UINT64_MAX);
        device.resetFences(1, &frame_data[current_frame]->fence);

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

        // After ray tracing is done, transition the image to a transfer source layout
        barrier = vk::ImageMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, frame_data[current_frame]->rt_image, subresource_range);
        cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eRayTracingShaderKHR, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr, nullptr, barrier);
        
        // acquire next swapchain image
        uint32_t swapchain_image_index;
        device.acquireNextImageKHR(swapchain->get_swapchain(), UINT64_MAX, frame_data[current_frame]->sc_image_available, nullptr, &swapchain_image_index);
        
        // Blit rt_image into swapchain image

        vk::ImageBlit blit(
            vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            { vk::Offset3D{0, 0, 0}, vk::Offset3D{r_width, r_height, 1} },
            vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            { vk::Offset3D{0, 0, 0}, vk::Offset3D{r_width, r_height, 1} }
        );
        vk::ImageSubresourceRange subresource_range_src(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        vk::ImageSubresourceRange subresource_range_dst(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        vk::ImageMemoryBarrier barrier_src(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, frame_data[current_frame]->rt_image, subresource_range_src);  
        vk::ImageMemoryBarrier barrier_dst(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapchain->get_image(swapchain_image_index), subresource_range_dst);
        cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eRayTracingShaderKHR, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr, nullptr, barrier_src);
        cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr, nullptr, barrier_dst);
        cmd_buffer.blitImage(frame_data[current_frame]->rt_image, vk::ImageLayout::eTransferSrcOptimal, swapchain->get_image(swapchain_image_index), vk::ImageLayout::eTransferDstOptimal, 1, &blit, vk::Filter::eNearest);
        barrier_src = vk::ImageMemoryBarrier(vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, frame_data[current_frame]->rt_image, subresource_range_src);
        barrier_dst = vk::ImageMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, swapchain->get_image(swapchain_image_index), subresource_range_dst);
        cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlags(), nullptr, nullptr, barrier_src);
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