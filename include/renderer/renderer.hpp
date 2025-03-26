#pragma once
#include <renderer/vulkan.hpp>

#include <renderer/swapchain.hpp>
#include <renderer/command_pool.hpp>
#include <renderer/pipeline.hpp>
#include <renderer/window/window_system_glfw.hpp>

#include <geometry/geometry.hpp>

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
    WindowHandle window;
    WindowSystemGLFW * window_system;

    vk::detail::DispatchLoaderDynamic dl;
    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    vk::Device device;

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
        int graphics_queue_family_index = -1;
        int present_queue_family_index = -1;

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
        create_pipeline();
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

        std::string rengen_module_path = "";
        std::string miss_module_path = "";
        std::string clhit_module_path = "";

        pipeline.push_module(rengen_module_path, vk::ShaderStageFlagBits::eRaygenKHR);
        pipeline.push_module(miss_module_path, vk::ShaderStageFlagBits::eMissKHR);
        pipeline.push_module(clhit_module_path, vk::ShaderStageFlagBits::eClosestHitKHR);
        pipeline.create_rt_pipeline(3);
    }

    void cleanup_vulkan() {
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

        setup_vulkan();

        swapchain = std::make_unique<Swapchain>(physical_device, device, window_system->get(window), surface);
        
        std::cout << "Renderer created" << std::endl;
    }

    ~Renderer() {
        if (swapchain) {
            swapchain.reset();
        }
        cleanup_vulkan();
        std::cout << "Renderer destroyed" << std::endl;
    }

    
    void load_scene(std::string file_path);

    void create_sbt();
};