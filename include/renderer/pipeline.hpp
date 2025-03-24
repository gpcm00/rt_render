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

struct PipelineModules {
    std::string file_path;
    VkShaderModule module;
    VkShaderStageFlagBits stage_flag;
};

class Pipeline {
    VkDevice* device;

    size_t nstage;
    std::vector<VkDescriptorSetLayout> sets;
    VkPipelineLayout layout;
    VkPipeline rt_pipeline;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<PipelineModules> modules;

    uint32_t miss_count;
    uint32_t chit_count;
    uint32_t intr_count;
    uint32_t ahit_count;

    VkShaderModule load_module(std::string file_name);
    void count_shader_module(VkShaderStageFlagBits stage_flag);

    public:
    Pipeline() = default;
    Pipeline(VkDevice* dev ) : device(dev) {
        miss_count = 0;
        chit_count = 0;
        intr_count = 0;
    };

    ~Pipeline() {
        for (auto& set : sets) {
            vkDestroyDescriptorSetLayout(*device, set, nullptr);
        }

        vkDestroyPipeline(*device, rt_pipeline, nullptr);
        vkDestroyPipelineLayout(*device, layout, nullptr);
    }
    
    void create_rt_pipeline(uint32_t ray_depth);
    void push_module(std::string file_name, VkShaderStageFlagBits stage_flag);
    void add_binding(VkDescriptorType type, VkShaderStageFlags flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR, uint32_t count = 1);
    void create_set();

    uint32_t shader_count() {
        return nstage;
    }

    VkPipeline pipeline_data() {
        return rt_pipeline;
    }
};