#pragma once
#include <fstream>
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <renderer/swapchain.hpp>

// NOTE(bc): https://github.com/KhronosGroup/Vulkan-Hpp?tab=readme-ov-file#extensions--per-device-function-pointers
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

class Renderer {
    private:
    WindowHandle window;
    WindowSystemGLFW * window_system;

    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    vk::Device device;

    vk::SurfaceKHR surface;
    std::unique_ptr<Swapchain> swapchain;
    
    vk::DescriptorSetLayout descriptor_set_layout;
    std::vector<vk::ShaderModule> shader_modules;
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shader_groups;
    vk::PipelineLayout pipeline_layout;
    vk::Pipeline pipeline;

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

        VULKAN_HPP_DEFAULT_DISPATCHER.init();

        vk::InstanceCreateInfo create_info({}, &app_info, validation_layers.size(), validation_layers.data(), extensions.size(), extensions.data());

        try {
            instance = vk::createInstance(create_info);
        } catch (const std::runtime_error& e) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }

        VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

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
        VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

        create_pipeline();
    }

    void create_pipeline() {
        vk::DescriptorSetLayoutBinding acceleration_structure_binding{};
        acceleration_structure_binding.binding = 0;
        acceleration_structure_binding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
        acceleration_structure_binding.descriptorCount = 1;
        acceleration_structure_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

        vk::DescriptorSetLayoutBinding result_image_binding{};
        result_image_binding.binding = 1;
        result_image_binding.descriptorType = vk::DescriptorType::eStorageImage;
        result_image_binding.descriptorCount = 1;
        result_image_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

        vk::DescriptorSetLayoutBinding uniform_buffer_binding{};
        uniform_buffer_binding.binding = 2;
        uniform_buffer_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        uniform_buffer_binding.descriptorCount = 1;
        uniform_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

        std::vector<vk::DescriptorSetLayoutBinding> bindings = {
            acceleration_structure_binding,
            result_image_binding,
            uniform_buffer_binding
        };

        vk::DescriptorSetLayoutCreateInfo desriptor_set_layout_create_info{};
        desriptor_set_layout_create_info.bindingCount = bindings.size();
        desriptor_set_layout_create_info.pBindings = bindings.data();

        descriptor_set_layout = device.createDescriptorSetLayout(desriptor_set_layout_create_info);

        vk::PipelineLayoutCreateInfo pipeline_layout_create_info{};
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout;

        pipeline_layout = device.createPipelineLayout(pipeline_layout_create_info);

        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;

        // TODO(bc): file names are hardcoded here for now, probably should be somewhere else

        shader_stages.push_back(load_shader("shaders/shader.rgen.spv", vk::ShaderStageFlagBits::eRaygenKHR));
        vk::RayTracingShaderGroupCreateInfoKHR raygen_shader_group_create_info{};
        raygen_shader_group_create_info.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
        raygen_shader_group_create_info.generalShader = shader_stages.size() - 1;
        shader_groups.push_back(raygen_shader_group_create_info);

        shader_stages.push_back(load_shader("shaders/shader.rmiss.spv", vk::ShaderStageFlagBits::eMissKHR));
        vk::RayTracingShaderGroupCreateInfoKHR miss_shader_group_create_info{};
        miss_shader_group_create_info.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
        miss_shader_group_create_info.generalShader = shader_stages.size() - 1;
        shader_groups.push_back(miss_shader_group_create_info);

        shader_stages.push_back(load_shader("shaders/shader.rchit.spv", vk::ShaderStageFlagBits::eClosestHitKHR));
        vk::RayTracingShaderGroupCreateInfoKHR closest_hit_shader_group_create_info{};
        closest_hit_shader_group_create_info.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
        closest_hit_shader_group_create_info.closestHitShader = shader_stages.size() - 1;
        shader_groups.push_back(closest_hit_shader_group_create_info);

        vk::RayTracingPipelineCreateInfoKHR raytracing_pipeline_create_info{};
        raytracing_pipeline_create_info.stageCount = shader_stages.size();
        raytracing_pipeline_create_info.pStages = shader_stages.data();
        raytracing_pipeline_create_info.groupCount = shader_groups.size();
        raytracing_pipeline_create_info.pGroups = shader_groups.data();
        raytracing_pipeline_create_info.maxPipelineRayRecursionDepth = 1;
        raytracing_pipeline_create_info.layout = pipeline_layout;
        auto pipelineResult = device.createRayTracingPipelineKHR(nullptr, {}, raytracing_pipeline_create_info);
        if (pipelineResult.result != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create pipeline");

        pipeline = pipelineResult.value;
    }

    vk::PipelineShaderStageCreateInfo load_shader(std::string filename, vk::ShaderStageFlagBits stage) {
        std::ifstream stream(filename, std::ios::ate | std::ios::binary);
        std::streamsize stream_size = stream.tellg();
        stream.seekg(0, std::ios::beg);
        std::vector<uint32_t> source(stream_size / sizeof(uint32_t));
        stream.read((char*)source.data(), stream_size);
        stream.close();

        vk::ShaderModuleCreateInfo shader_module_create_info{};
        shader_module_create_info.codeSize = source.size() * sizeof(uint32_t);
        shader_module_create_info.pCode = source.data();

        vk::ShaderModule shader_module = device.createShaderModule(shader_module_create_info);
        shader_modules.push_back(shader_module);

        vk::PipelineShaderStageCreateInfo shader_stage{};
        shader_stage.stage = stage;
        shader_stage.module = shader_module;
        shader_stage.pName = "main";

        return shader_stage;
    }

    void cleanup_vulkan() {
        device.destroyDescriptorSetLayout(descriptor_set_layout);
        device.destroyPipelineLayout(pipeline_layout);
        for (auto& shader_module : shader_modules)
            device.destroyShaderModule(shader_module);
        device.destroyPipeline(pipeline);
        device.destroy();
        instance.destroySurfaceKHR(surface);
        instance.destroy();
    }  

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



};