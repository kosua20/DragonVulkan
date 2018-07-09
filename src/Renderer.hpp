#pragma once
#include "VulkanUtilities.hpp"


struct GLFWwindow;

class Renderer
{
public:

	Renderer(VkInstance & instance, VkSurfaceKHR & surface);

	~Renderer();

	int init(const int width, const int height);
	
	void cleanup();
	
	VkResult draw();
	
	int recreateSwapchain(const int width, const int height);

protected:
	
	int createSwapchain(const int width, const int height);
	void cleanupSwapChain();
	
private:

	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	
	VkCommandPool commandPool;
	VkSwapchainKHR swapChain;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;

	std::vector<VkImage> swapChainImages;
	VkRenderPass renderPass;
	VkPipeline graphicsPipeline;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	std::vector<VkCommandBuffer> commandBuffers;
	VkPipelineLayout pipelineLayout;
	VulkanUtilities::ActiveQueues queues;
	bool debugEnabled;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	size_t currentFrame = 0;
	bool framebufferResized = false;
	VkQueue graphicsQueue, presentQueue;
};

