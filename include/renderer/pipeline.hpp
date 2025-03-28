#pragma once
#include <iostream>
#include <renderer/vulkan.hpp>
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
    vk::ShaderModule module;
    vk::ShaderStageFlagBits stage_flag;
};

class Pipeline {
    vk::Device& device;
    vk::detail::DispatchLoaderDynamic dl;

    size_t nstage;
    std::vector<vk::DescriptorSetLayout> sets;
    vk::PipelineLayout layout;
    vk::Pipeline rt_pipeline;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;

    std::vector<PipelineModules> modules;

    uint32_t miss_count;
    uint32_t chit_count;
    uint32_t intr_count;
    uint32_t ahit_count;

    vk::ShaderModule load_module(std::string file_name);
    void count_shader_module(vk::ShaderStageFlagBits stage_flag);

    public:
    Pipeline() = default;

    Pipeline(vk::Device& device, vk::detail::DispatchLoaderDynamic & dl) 
    : device(device), dl(dl) {
        miss_count = 0;
        chit_count = 0;
        intr_count = 0;
    };

    ~Pipeline() {
        for (auto& set : sets) {
            device.destroyDescriptorSetLayout(set);
        }

        device.destroyPipeline(rt_pipeline);
        device.destroyPipelineLayout(layout);
    }
    
    void create_rt_pipeline(uint32_t ray_depth);
    void push_module(std::string file_name, vk::ShaderStageFlagBits stage_flag);
    void add_binding(vk::DescriptorType type, vk::ShaderStageFlags flags = vk::ShaderStageFlagBits::eRaygenKHR, uint32_t count = 1);
    void create_set();

    uint32_t shader_count() {
        return nstage;
    }

    vk::Pipeline & get_pipeline() {
        return rt_pipeline;
    }

    vk::DescriptorSetLayout* get_set() {
        return sets.data();
    }
};