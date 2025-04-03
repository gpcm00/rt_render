#include <renderer/vulkan.hpp>
#include <geometry/geometry.hpp>
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

    

    std::vector<Textures> images_memory;

    vk::ImageCreateInfo get_create_info(uint32_t width, uint32_t height) {
        vk::ImageCreateInfo imageInfo{};
        imageInfo.sType = vk::StructureType::eImageCreateInfo;
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = vk::Format::eR8G8B8A8Unorm;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        imageInfo.usage = vk::ImageUsageFlagBits::eSampled;
        imageInfo.samples = vk::SampleCountFlagBits::e1; 
        imageInfo.sharingMode = vk::SharingMode::eExclusive;

        return imageInfo;
    }
};