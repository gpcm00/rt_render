#include <iostream>
#include <renderer/app.hpp>
#include <renderer/window/window_system_glfw.hpp>
#include <renderer/input/input_system.hpp>
#include <renderer/input/keyboard_glfw.hpp>
#include <renderer/input/mouse_glfw.hpp>
#include <memory>

#include <vulkan/vulkan.hpp>
class Renderer {
    private:
    WindowHandle window;
    WindowSystemGLFW * window_system;

    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    vk::Device device;
    

    vk::SurfaceKHR surface;

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

    public:

    Renderer(WindowHandle window, WindowSystemGLFW * window_system): window(window), window_system(window_system) {

        setup_vulkan();
        std::cout << "Renderer created" << std::endl;
    }

    ~Renderer() {
        cleanup_vulkan();
        std::cout << "Renderer destroyed" << std::endl;
    }



};

class PathTracer: public App {
private:
    WindowSystemGLFW window_system;
    InputSystem input_system;
    std::unique_ptr<Renderer> renderer;

    std::function<void()> exit_function;

public:
    PathTracer(): input_system(&window_system, new KeyboardGLFW(&window_system), new MouseGLFW(&window_system)) {
        std::cout << "PathTracer created" << std::endl;

        auto window = window_system.create_window(800, 600);
        window_system.set_title(window.value(), "Vulkan Path Tracer");

        renderer = std::make_unique<Renderer>(window.value(), &window_system);
    }

    ~PathTracer() {
        std::cout << "PathTracer destroyed" << std::endl;
    }

    void set_exit_function(std::function<void()> function) override {
        exit_function = function;
    }

    void fixed_update(const FrameConstants & frame_constants) override {
        input_system.update();

        if (input_system.get_button_state("Exit") == input::ButtonState::Pressed || input_system.get_button_state("Exit") == input::ButtonState::Held) {
            exit_function();
        }
    }

    void render_update(const FrameConstants & frame_constants) override {
    }
};

int main() {

    PathTracer().run();

    return 0;
}