#pragma once
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <vector>
#include <cassert>
#include <glm/glm.hpp>
#include <algorithm>

struct RayConstants {
    glm::vec4 color;
    glm::vec3 position;
    float intensity;
    int type;
};

class Pipeline {
    VkDevice* device;
    size_t nstage;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    VkPipelineLayout layout;
    VkPipeline rt_pipeline;

    public:
    Pipeline(VkDevice* dev) : device(dev) {};
    VkPipeline create_rt_pipeline(std::vector<VkShaderModule> modules, uint32_t ray_depth) {
        
        std::vector<VkPipelineShaderStageCreateInfo> stages{};
        
        /*
            if we want to build a hybrid pipeline,
            add them before raygen in the enum Pipeline_stage

            also add the VkShaderStageFlagBits in the pipeline_stages[]

            finally add the configurations for the rasterizers stages added
        */

        enum Pipeline_stage {
            // add vertex stage, fragment stage, etc
            raygen,
            raymiss,
            rayclhit,
            rayinter,
            rayanyhit,
            maxstages,
        };

        VkShaderStageFlagBits pipeline_stages[] = {
            VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            VK_SHADER_STAGE_MISS_BIT_KHR,
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
            VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        };

        nstage = 0;
        for (auto& module : modules) {
            VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            stage.pName = "main";  
            stage.module = module;
            stage.stage = pipeline_stages[nstage];

            stages.push_back(stage);
            nstage++;
        }

        groups.resize(nstage-raygen);

        uint32_t group_idx = raygen;
        for (auto& group : groups) {
            group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            group.generalShader = (group_idx < rayclhit)? group_idx : VK_SHADER_UNUSED_KHR; 
            group.closestHitShader = (group_idx == rayclhit)? group_idx : VK_SHADER_UNUSED_KHR;
            group.anyHitShader = (group_idx == rayinter)? group_idx : VK_SHADER_UNUSED_KHR;
            group.intersectionShader = (group_idx == rayanyhit)? group_idx : VK_SHADER_UNUSED_KHR;

            group_idx++;
        }

        // TODO: Layout with rasterization
        // VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        // VkPushConstantRange constants = {
        //     .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
        //     .offset = 0, 
        //     .size = sizeof(RayConstants),
        // };

        // layout_info.pushConstantRangeCount = 1;
        // layout_info.pPushConstantRanges = &constants;

        // std::vector<VkDescriptorSetLayout> layouts = {}

        VkRayTracingPipelineCreateInfoKHR pipeline_info{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
        pipeline_info.stageCount = nstage;
        pipeline_info.pStages = stages.data();
        pipeline_info.groupCount = static_cast<uint32_t>(groups.size());
        pipeline_info.pGroups = groups.data();
        pipeline_info.maxPipelineRayRecursionDepth = ray_depth;
        // pipeline_info.layout = TODO
        vkCreateRayTracingPipelinesKHR(*device, nullptr, nullptr, 1, &pipeline_info, nullptr, &rt_pipeline);
    }
};