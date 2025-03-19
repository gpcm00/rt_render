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
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    std::vector<VkDescriptorSetLayout> sets;
    VkPipelineLayout layout;
    VkPipeline rt_pipeline;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<PipelineModules> modules;

    void add_binding(VkDescriptorType type, VkShaderStageFlags flags, uint32_t count = 1);
    void create_set();

    VkShaderModule load_module(std::string file_name);

    public:
    Pipeline(VkDevice* dev) : device(dev) {};
    ~Pipeline() {
        for (auto& set : sets) {
            vkDestroyDescriptorSetLayout(*device, set, nullptr);
        }  
    }
    
    VkPipeline create_rt_pipeline(uint32_t ray_depth);
    void push_module(std::string file_name, VkShaderStageFlagBits stage_flag);
};