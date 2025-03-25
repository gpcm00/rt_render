#pragma once
#include <renderer/vulkan.hpp>
#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Swapchain {
private:
	vk::UniqueSwapchainKHR swapchain;
	std::vector<vk::Image> images;
	std::vector<vk::ImageView> image_views;
	vk::Format format;
	vk::Extent2D extent;
	std::vector<vk::Framebuffer> framebuffers;
	vk::Device * device;

public:
		Swapchain(vk::PhysicalDevice & physical_device, vk::Device & device, GLFWwindow * window, vk::SurfaceKHR & surface);
		unsigned int get_num_images();
		uint32_t get_width();
		uint32_t get_height();
		vk::Extent2D & get_extent();
		vk::Format & get_format();

		vk::ImageView & get_image_view(unsigned int index);
		vk::Framebuffer & get_frame_buffer(unsigned int index);
		vk::SwapchainKHR get_swapchain();

		~Swapchain();

		void create_framebuffers(vk::UniqueDevice & device, const vk::RenderPass & renderPass, vk::ImageView depthImageView);
		void destroy_framebuffers();
};
