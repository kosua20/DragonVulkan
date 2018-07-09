#pragma once
#include "VulkanUtilities.hpp"
#include <glm/glm.hpp>


struct GLFWwindow;

class Renderer
{
public:

	Renderer(VkInstance & instance, VkSurfaceKHR & surface);

	~Renderer();

	int init(const int width, const int height);
	
	void cleanup();
	
	VkResult draw();
	
	int resize(const int width, const int height);

protected:
	
	int createSwapchain(const int width, const int height);
	
	int fillSwapchain(VkRenderPass & renderPass);
	int createMainRenderpass();
	int createPipeline(VkPipelineShaderStageCreateInfo * shaderStages);
	int generateCommandBuffers();
	
	void cleanupSwapChain();
	
private:
	
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	
	VkPipeline graphicsPipeline;
	VkPipelineLayout pipelineLayout;
	VkRenderPass mainRenderPass;
	
	VulkanUtilities::ActiveQueues queues;
	VkQueue graphicsQueue, presentQueue;
	
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	
	VkSwapchainKHR swapChain;
	VulkanUtilities::SwapchainParameters swapchainParams;
	
	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	
	size_t currentFrame = 0;
	glm::vec2 _size = glm::vec2(0.0f,0.0f);
};

