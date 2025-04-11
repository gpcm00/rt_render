#include <geometry/geometry.hpp>
#include <renderer/vulkan.hpp>
#include <vector>


class ImageStorage {
  public:
    struct Textures {
        vk::Image image;
        vk::DeviceSize size;
        VmaAllocation memory;
        vk::ImageView view;
        vk::Sampler sampler;
    };

    std::vector<Textures> base_color_textures;
    std::vector<Textures> normal_textures;
    std::vector<Textures> metallic_roughness_textures;
    std::vector<Textures> emissive_textures;
    std::vector<Textures> occlusion_textures;

    vk::ImageCreateInfo get_create_info(uint32_t width, uint32_t height,
                                        vk::Format format) {
        vk::ImageCreateInfo imageInfo{};
        imageInfo.sType = vk::StructureType::eImageCreateInfo;
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        imageInfo.usage = vk::ImageUsageFlagBits::eSampled;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.sharingMode = vk::SharingMode::eExclusive;

        return imageInfo;
    }
};
