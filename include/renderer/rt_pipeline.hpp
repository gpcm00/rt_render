#pragma once
#include <fstream>
#include <renderer/vulkan.hpp>

class RTPipeline {
  private:
    vk::ShaderModule load_module(std::string file_name) {
        std::ifstream file(file_name, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open " + file_name);
        }

        std::streampos size = file.tellg();
        if (size == std::streampos(-1)) {
            throw std::runtime_error("Failed to read the size of " + file_name);
        }

        std::vector<char> buffer(static_cast<size_t>(size));

        file.seekg(0);
        file.read(buffer.data(), size);

        file.close();

        vk::ShaderModuleCreateInfo createInfo{};
        createInfo.codeSize = buffer.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(buffer.data());

        vk::ShaderModule shaderModule;
        if (device.createShaderModule(&createInfo, nullptr, &shaderModule) !=
            vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create shader module: " +
                                     file_name);
        }

        return shaderModule;
    }

  public:
    vk::Device &device;
    VmaAllocator &allocator;
    vk::detail::DispatchLoaderDynamic &dl;
    vk::Pipeline pipeline;
    vk::PipelineLayout layout;
    vk::DescriptorSetLayout descriptor_set_layout;

    RTPipeline(vk::Device &device, VmaAllocator &allocator,
               vk::detail::DispatchLoaderDynamic &dl,
               const std::vector<vk::DescriptorSetLayoutBinding> &bindings,
               std::string rgen_path, const std::string miss_path,
               const std::string chit_path)
        : device(device), allocator(allocator), dl(dl) {
        std::cout << "Creating pipeline" << std::endl;

        std::vector<vk::ShaderModule> modules;

        if (!rgen_path.empty()) {
            modules.push_back(load_module(rgen_path));
        }
        if (!miss_path.empty()) {
            modules.push_back(load_module(miss_path));
        }
        if (!chit_path.empty()) {
            modules.push_back(load_module(chit_path));
        }

        if (modules.size() != 3) {
            throw std::runtime_error(
                "Not enough shader modules provided for pipeline creation");
        }

        // Create descriptor set layout
        vk::DescriptorSetLayoutCreateInfo ds_info;
        ds_info.bindingCount = static_cast<uint32_t>(bindings.size());
        ds_info.pBindings = bindings.data();
        ds_info.flags = vk::DescriptorSetLayoutCreateFlags();
        device.createDescriptorSetLayout(&ds_info, nullptr,
                                         &descriptor_set_layout);

        vk::DescriptorBindingFlags binding_flags =
            vk::DescriptorBindingFlagBits::ePartiallyBound |
            vk::DescriptorBindingFlagBits::eVariableDescriptorCount |
            vk::DescriptorBindingFlagBits::eUpdateAfterBind;

        vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info;
        std::vector<vk::DescriptorBindingFlags> flags(bindings.size(),
                                                      binding_flags);
        binding_flags_info.bindingCount =
            static_cast<uint32_t>(bindings.size());
        binding_flags_info.pBindingFlags = flags.data();

        // Create pipeline layout
        vk::PipelineLayoutCreateInfo layout_info;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &descriptor_set_layout;
        layout_info.pushConstantRangeCount = 1;
        vk::PushConstantRange push_constant_range;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(PushConstant);
        push_constant_range.stageFlags =
            vk::ShaderStageFlagBits::eRaygenKHR |
            vk::ShaderStageFlagBits::eMissKHR |
            vk::ShaderStageFlagBits::eClosestHitKHR;
        layout_info.pPushConstantRanges = &push_constant_range;
        layout_info.pNext = &binding_flags_info;

        if (device.createPipelineLayout(&layout_info, nullptr, &layout) !=
            vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        // Create pipeline
        vk::PipelineShaderStageCreateInfo shader_stages[3] = {
            {vk::PipelineShaderStageCreateFlags(),
             vk::ShaderStageFlagBits::eRaygenKHR, modules[0], "main"},
            {vk::PipelineShaderStageCreateFlags(),
             vk::ShaderStageFlagBits::eMissKHR, modules[1], "main"},
            {vk::PipelineShaderStageCreateFlags(),
             vk::ShaderStageFlagBits::eClosestHitKHR, modules[2], "main"}};

        // Create shader groups for each stage
        std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groups(3);
        groups[0].type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
        groups[0].generalShader = 0;
        groups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
        groups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
        groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

        groups[1].type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
        groups[1].generalShader = 1;
        groups[1].closestHitShader = VK_SHADER_UNUSED_KHR;
        groups[1].anyHitShader = VK_SHADER_UNUSED_KHR;
        groups[1].intersectionShader = VK_SHADER_UNUSED_KHR;

        groups[2].type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
        groups[2].generalShader = VK_SHADER_UNUSED_KHR;
        groups[2].closestHitShader = 2;
        groups[2].anyHitShader = VK_SHADER_UNUSED_KHR;
        groups[2].intersectionShader = VK_SHADER_UNUSED_KHR;

        // Finally, create the pipeline
        vk::RayTracingPipelineCreateInfoKHR pipeline_info;
        pipeline_info.stageCount = 3;
        pipeline_info.pStages = shader_stages;
        pipeline_info.groupCount = 3;
        pipeline_info.maxPipelineRayRecursionDepth = 31; // Adjust later
        pipeline_info.layout = layout;
        pipeline_info.pGroups = groups.data();

        auto ret = device.createRayTracingPipelineKHR(
            nullptr, nullptr, pipeline_info, nullptr, dl);
        if (ret.result != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create ray tracing pipeline");
        }
        pipeline = ret.value;

        // Delete shader modules after pipeline creation
        for (auto &module : modules) {
            device.destroyShaderModule(module);
        }
    }

    ~RTPipeline() {
        std::cout << "Destroying pipeline" << std::endl;
        device.destroyPipeline(pipeline);
        device.destroyPipelineLayout(layout);
        device.destroyDescriptorSetLayout(descriptor_set_layout);
    }
};
