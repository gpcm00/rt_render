#include <pipeline.hpp>
#include <fstream>

static VkRayTracingShaderGroupCreateInfoKHR populate_group(VkShaderStageFlagBits stage_flag, uint32_t index) {
    VkRayTracingShaderGroupCreateInfoKHR group{};
    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = VK_SHADER_UNUSED_KHR; 
    group.closestHitShader = VK_SHADER_UNUSED_KHR;
    group.anyHitShader = VK_SHADER_UNUSED_KHR;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;

    switch (stage_flag) {
        case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
        case VK_SHADER_STAGE_MISS_BIT_KHR:
            group.generalShader = index;
            break;

        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
            group.closestHitShader = index;
            break;

        case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
            group.intersectionShader = index;
            break;

        case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
            group.anyHitShader = index;
        
        default:
            break;
    }

    return group;
}

VkShaderModule Pipeline::load_module(std::string file_name) {
    std::ifstream file(file_name, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open " + file_name);
    }

    std::streampos size = file.tellg();
    if(size == std::streampos(-1)) {
        throw std::runtime_error("Failed to read the size of " + file_name);
    }

    std::vector<char> buffer(static_cast<size_t>(size));

    file.seekg(0);
    file.read(buffer.data(), size);

    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(*device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void Pipeline::push_module(std::string file_path, VkShaderStageFlagBits flag) {
    PipelineModules module = {
        .file_path = file_path,
        .module = load_module(file_path),
        .stage_flag = flag
    };

    modules.push_back(module);
}

void Pipeline::add_binding(VkDescriptorType type, VkShaderStageFlags flags, uint32_t count) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = bindings.size();
    binding.descriptorType = type;
    binding.stageFlags = flags;
    binding.descriptorCount = count;
    bindings.push_back(binding);
}

void Pipeline::create_set() {
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();

    VkDescriptorSetLayout set;
    if (vkCreateDescriptorSetLayout(*device, &info, nullptr, &set) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing descriptor set layout!");
    }

    sets.push_back(set);
    bindings.clear();
}

VkPipeline Pipeline::create_rt_pipeline(uint32_t ray_depth) {
        
    std::vector<VkPipelineShaderStageCreateInfo> stages{};

    nstage = 0;
    for (auto& module : modules) {
        VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stage.pName = "main";  
        stage.module = module.module;
        stage.stage = module.stage_flag;

        stages.push_back(stage);

        VkRayTracingShaderGroupCreateInfoKHR group = populate_group(module.stage_flag, nstage);
        groups.push_back(group);

        nstage++;
    }

    VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkPushConstantRange constants = {
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
        .offset = 0, 
        .size = sizeof(RayConstants),
    };

    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &constants;
    layout_info.setLayoutCount = sets.size();
    layout_info.pSetLayouts = sets.data();
    if (vkCreatePipelineLayout(*device, &layout_info, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkRayTracingPipelineCreateInfoKHR pipeline_info{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
    pipeline_info.stageCount = nstage;
    pipeline_info.pStages = stages.data();
    pipeline_info.groupCount = static_cast<uint32_t>(groups.size());
    pipeline_info.pGroups = groups.data();
    pipeline_info.maxPipelineRayRecursionDepth = ray_depth;
    pipeline_info.layout = layout;
    if (vkCreateRayTracingPipelinesKHR(*device, nullptr, nullptr, 1, &pipeline_info, nullptr, &rt_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline");
    }
}