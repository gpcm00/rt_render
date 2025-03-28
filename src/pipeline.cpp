#include <renderer/pipeline.hpp>
#include <fstream>

static vk::WriteDescriptorSet populate_write_descriptor(vk::Buffer buffer, vk::DeviceSize size, vk::DescriptorSet set, vk::DescriptorType type, uint32_t binding) {
    vk::DescriptorBufferInfo bi{};
    bi.buffer = buffer;
    bi.range = size;
    
    vk::WriteDescriptorSet wds{};
    wds.dstSet = set;
    wds.descriptorCount = 1;
    wds.dstBinding = binding;
    wds.pBufferInfo = &bi;
    wds.descriptorType = type;

    return wds;
}

static vk::RayTracingShaderGroupCreateInfoKHR populate_group(vk::ShaderStageFlagBits stage_flag, uint32_t index) {
    vk::RayTracingShaderGroupCreateInfoKHR group{};
    group.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    group.generalShader = vk::ShaderUnusedKHR; 
    group.closestHitShader =  vk::ShaderUnusedKHR;
    group.anyHitShader =  vk::ShaderUnusedKHR;
    group.intersectionShader =  vk::ShaderUnusedKHR;

    switch (stage_flag) {
        case vk::ShaderStageFlagBits::eRaygenKHR:
        case vk::ShaderStageFlagBits::eMissKHR:
            group.generalShader = index;
            break;

        case vk::ShaderStageFlagBits::eClosestHitKHR:
            group.closestHitShader = index;
            break;

        case vk::ShaderStageFlagBits::eIntersectionKHR:
            group.intersectionShader = index;
            break;

        case vk::ShaderStageFlagBits::eAnyHitKHR:
            group.anyHitShader = index;
        
        default:
            break;
    }

    return group;
}

vk::ShaderModule Pipeline::load_module(std::string file_name) {
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

    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    vk::ShaderModule shaderModule;
    if (device.createShaderModule(&createInfo, nullptr, &shaderModule) != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to create shader module: " + file_name);
    }

    return shaderModule;
}

void Pipeline::count_shader_module(vk::ShaderStageFlagBits stage_flag) {
    switch (stage_flag)
    {
    case vk::ShaderStageFlagBits::eMissKHR:
        miss_count++;
        break;
    
    case vk::ShaderStageFlagBits::eClosestHitKHR:
        chit_count++;
        break;
    
    case vk::ShaderStageFlagBits::eIntersectionKHR:
        intr_count++;
        break;

    case vk::ShaderStageFlagBits::eAnyHitKHR:
        ahit_count++;
        break;
    
    default:
        break;
    }
}

void Pipeline::push_module(std::string file_path, vk::ShaderStageFlagBits flag) {
    PipelineModules module = {
        .file_path = file_path,
        .module = load_module(file_path),
        .stage_flag = flag
    };

    modules.push_back(module);
}

void Pipeline::add_binding(vk::DescriptorType type, vk::ShaderStageFlags flags, uint32_t count) {
    vk::DescriptorSetLayoutBinding binding{};
    binding.binding = bindings.size();
    binding.descriptorType = type;
    binding.stageFlags = flags;
    binding.descriptorCount = count;
    bindings.push_back(binding);
}

void Pipeline::create_set() {
    vk::DescriptorSetLayoutCreateInfo info{};
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();

    vk::DescriptorSetLayout set;
    if (device.createDescriptorSetLayout(&info, nullptr, &set) != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to create ray tracing descriptor set layout!");
    }

    sets.push_back(set);
    bindings.clear();
}

void Pipeline::create_rt_pipeline(uint32_t ray_depth) {
        
    std::vector<vk::PipelineShaderStageCreateInfo> stages{};
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groups{};

    nstage = 0;
    for (auto& module : modules) {
        vk::PipelineShaderStageCreateInfo stage{};
        stage.pName = "main";  
        stage.module = module.module;
        stage.stage = module.stage_flag;

        stages.push_back(stage);

        vk::RayTracingShaderGroupCreateInfoKHR group = populate_group(module.stage_flag, nstage);
        groups.push_back(group);

        count_shader_module(module.stage_flag);

        nstage++;
    }

    vk::PipelineLayoutCreateInfo layout_info{};
    vk::PushConstantRange constants;
    constants.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR;
    constants.offset = 0;
    constants.size = sizeof(RayConstants);


    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &constants;
    layout_info.setLayoutCount = sets.size();
    layout_info.pSetLayouts = sets.data();
    if (device.createPipelineLayout(&layout_info, nullptr, &layout) != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    vk::RayTracingPipelineCreateInfoKHR pipeline_info{};
    pipeline_info.stageCount = nstage;
    pipeline_info.pStages = stages.data();
    pipeline_info.groupCount = static_cast<uint32_t>(groups.size());
    pipeline_info.pGroups = groups.data();
    pipeline_info.maxPipelineRayRecursionDepth = ray_depth;
    pipeline_info.layout = layout;

    auto ret = device.createRayTracingPipelineKHR(nullptr, nullptr, pipeline_info, nullptr, dl);
    if (ret.result != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to create pipeline");
    }

    rt_pipeline = ret.value;

    for (auto& module : modules) {
        device.destroyShaderModule(module.module);
    }

}