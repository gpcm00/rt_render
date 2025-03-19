#pragma once
#include <vulkan/vulkan.hpp>
#include <renderer/swapchain.hpp>
#include <renderer/command_pool.hpp>
#include <renderer/pipeline.hpp>
#include <geometry/geometry.hpp>
#include <unordered_map>

struct MeshBuffer {
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;

    VkBuffer index_buffer;
    VkDeviceMemory index_memory;
    
    uint32_t size;
};


struct InstanceBuffer {
    MeshBuffer* mesh_buffer;
    VkBuffer buffer;
    VkDeviceMemory memory;
};

class Renderer {
    private:
    WindowHandle window;
    WindowSystemGLFW * window_system;

    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    vk::Device device;

    vk::SurfaceKHR surface;
    std::unique_ptr<Swapchain> swapchain;

    std::unordered_map<const Mesh*,MeshBuffer> meshes{};
    std::vector<InstanceBuffer> objects{};

    Command_Pool command;
    Pipeline pipeline;

    Scene scene;

    void setup_vulkan() {
        // Create Vulkan 1.3 instance  with validation layers
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

        vk::DeviceCreateInfo device_create_info({}, queue_create_infos.size(), queue_create_infos.data(), 0, nullptr, device_extensions.size(), device_extensions.data(), &device_features);

        device = physical_device.createDevice(device_create_info);
    }

    void cleanup_vulkan() {
        device.destroy();
        instance.destroySurfaceKHR(surface);
        instance.destroy();
    }  

    void create_buffer(VkBuffer& buffer, VkDeviceMemory& memory, VkDeviceSize size, VkBufferUsageFlags usage, 
                                VkMemoryPropertyFlags properties, VkSharingMode mode = VK_SHARING_MODE_EXCLUSIVE);
    
    void create_mesh_buffer(const Mesh* mesh);
    
    void create_device_buffer(VkBuffer& buffer, VkDeviceMemory& memory, const void* vertices, VkDeviceSize size, VkBufferUsageFlagBits usage);
    // void create_vertex_buffer(VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, std::vector<Vertex>& vertex);
    // void Renderer::create_index_buffer(VkBuffer& buffer, VkDeviceMemory& memory, std::vector<uint32_t>& vertices);
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
};