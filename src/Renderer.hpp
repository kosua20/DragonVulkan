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
	int createPipeline();
	int generateCommandBuffers();
	
	void cleanupSwapChain();
	
private:
	
	VkInstance _instance;
	VkSurfaceKHR _surface;
	VkPhysicalDevice _physicalDevice;
	VkDevice _device;
	
	VkPipeline _graphicsPipeline;
	VkPipelineLayout _pipelineLayout;
	VkRenderPass _mainRenderPass;
	
	VkQueue _graphicsQueue, _presentQueue;
	
	VkCommandPool _commandPool;
	std::vector<VkCommandBuffer> _commandBuffers;
	
	VkBuffer _vertexBuffer;
	VkDeviceMemory _vertexBufferMemory;
	VkBuffer _indexBuffer;
	VkDeviceMemory _indexBufferMemory;
	
	VkSwapchainKHR _swapchain;
	VulkanUtilities::SwapchainParameters _swapchainParams;
	
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	std::vector<VkFramebuffer> _swapchainFramebuffers;
	
	std::vector<VkSemaphore> _imageAvailableSemaphores;
	std::vector<VkSemaphore> _renderFinishedSemaphores;
	std::vector<VkFence> _inFlightFences;
	
	size_t _currentFrame = 0;
	glm::vec2 _size = glm::vec2(0.0f,0.0f);
};

