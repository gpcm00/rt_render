#include <renderer/swapchain.hpp>
#include <algorithm>

#include <iostream>

Swapchain::Swapchain(vk::PhysicalDevice & physical_device, vk::Device & device, GLFWwindow * window, vk::SurfaceKHR & surface)
{
	// get swap chain support
	const auto capabilities = physical_device.getSurfaceCapabilitiesKHR(surface);
	const auto formats = physical_device.getSurfaceFormatsKHR(surface);
	const auto present_modes = physical_device.getSurfacePresentModesKHR(surface);

	// Give us what we want or crash >:( (for now)
	const auto surface_format = vk::SurfaceFormatKHR(vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear);
	const auto present_mode = vk::PresentModeKHR::eFifo;
	
	// Grab the extent from the glfw window
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	// clamp the width and height to the min and max extents
	extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent.width));
	extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, extent.height));
	
	// we also have to decide how many images we would like to have in the swap chain (take an extra b/c sometimes have to wait for driver)
	uint32_t image_count = capabilities.minImageCount + 1;
	// make sure we're not exceeding the max count
	if (capabilities.maxImageCount > 0) {
		image_count = std::min(image_count, capabilities.maxImageCount);
	}

	// find the queue families that support graphics and presentation
	const auto queue_family_properties = physical_device.getQueueFamilyProperties();

	int graphics_family_index = -1;
	int present_family_index = -1;

	// assume graphics and presentation are the same queue family
	for (uint32_t i = 0; i < queue_family_properties.size(); i++) {
		if (queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) {
			graphics_family_index = i;
			present_family_index = i;
			break;
		}
	}

	if (graphics_family_index == -1 || present_family_index == -1) {
		throw std::runtime_error("Failed to find suitable queue families!");
	}

	std::cout << "Graphics family index: " << graphics_family_index << std::endl;	
	std::cout << "Present family index: " << present_family_index << std::endl;

	uint32_t queueFamilyIndices[2] = { 
		static_cast<uint32_t>(graphics_family_index), static_cast<uint32_t>(present_family_index) };


	// create the swap chain
	vk::SwapchainCreateInfoKHR create_info(vk::SwapchainCreateFlagsKHR(), surface, image_count, 
		surface_format.format, surface_format.colorSpace, extent, 1, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment,
		vk::SharingMode::eExclusive, 0, nullptr, capabilities.currentTransform, 
		vk::CompositeAlphaFlagBitsKHR::eOpaque, present_mode, true, nullptr);

	if (graphics_family_index != present_family_index) {
		create_info.imageSharingMode = vk::SharingMode::eConcurrent;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		create_info.imageSharingMode = vk::SharingMode::eExclusive;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices = nullptr;
	}

	create_info.setPreTransform(capabilities.currentTransform); // we want the current transform (don't want to change/flip/rotate
	create_info.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque); // no blending with other windows in window system (ignore alpha channel)
	create_info.setPresentMode(present_mode);
	create_info.setClipped(VK_TRUE); // ignore obscured pixels when window is in front
	create_info.setOldSwapchain(nullptr);

	// before creating swap chain, query for WSI support ( validation complains without this, I don't even know what this means)
	if (physical_device.getSurfaceSupportKHR(queueFamilyIndices[0], surface) != VK_TRUE) {
		throw std::runtime_error("This physical device does not include WSI support!");
	}

	format = surface_format.format;

	swapchain =  device.createSwapchainKHRUnique(create_info);
	
	// Get swap chain images and create image views
	images = device.getSwapchainImagesKHR(swapchain.get());
	image_views.resize(images.size());
	for (size_t i = 0; i < images.size(); i++) {
		vk::ImageViewCreateInfo iv_create_info({}, images[i], vk::ImageViewType::e2D, format, vk::ComponentMapping(vk::ComponentSwizzle::eIdentity), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
		image_views[i] = device.createImageView(iv_create_info);
	}

	// Store device for swapchain destructor.
	this->device = &device;
}
unsigned int Swapchain::get_num_images()
{
	return image_views.size();
}
uint32_t Swapchain::get_width()
{
	return extent.width;
}
uint32_t Swapchain::get_height()
{
	return extent.height;
}
vk::Extent2D & Swapchain::get_extent()
{
	return extent;
}
vk::Format & Swapchain::get_format()
{
	return format;
}
vk::Image & Swapchain::get_image(unsigned int index)
{
	return images.at(index);
}

vk::ImageView & Swapchain::get_image_view(unsigned int index)
{
	return image_views.at(index);
}
vk::Framebuffer & Swapchain::get_frame_buffer(unsigned int index)
{
	return framebuffers.at(index);
}
vk::SwapchainKHR & Swapchain::get_swapchain()
{
	return swapchain.get();
}

Swapchain::~Swapchain()
{
	
	if (!framebuffers.empty()) {
		destroy_framebuffers();
	}

	for (auto & view : image_views) {
		device->destroyImageView(view);
	}

	image_views.clear();
}
void Swapchain::create_framebuffers(vk::UniqueDevice & device, const vk::RenderPass & render_pass, vk::ImageView depth_image_view)
{
	const auto num_images = get_num_images();
	framebuffers.resize(num_images);
	for (unsigned int i = 0; i < num_images; i++) {
		std::vector<vk::ImageView> attachments = { get_image_view(i), depth_image_view };
		vk::FramebufferCreateInfo fbCI({}, render_pass, static_cast<uint32_t>(attachments.size()), attachments.data(), get_width(), get_height(), 1);
		framebuffers[i] = device->createFramebuffer(fbCI);
	}
}
void Swapchain::destroy_framebuffers()
{
	for (auto & framebuffer : framebuffers) {
		device->destroyFramebuffer(framebuffer);
	}
	framebuffers.clear();
}