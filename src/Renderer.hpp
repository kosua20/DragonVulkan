#pragma once
#include <vulkan/vulkan.hpp>

#include <set>

struct GLFWwindow;

class Renderer
{
public:

	Renderer();

	~Renderer();

	int init(GLFWwindow * window, const bool enableValidationLayers);
	
	void cleanup();
	
	int draw(GLFWwindow * window);
	
protected:
	bool isDeviceSuitable(VkPhysicalDevice adevice, VkSurfaceKHR asurface);
	int createSwapchain(GLFWwindow* window);
	int recreateSwapchain(GLFWwindow* window);
	void cleanupSwapChain();
	struct ActiveQueues{
		int graphicsQueue = -1;
		int presentQueue = -1;

		const bool isComplete() const {
			return graphicsQueue >= 0 && presentQueue >= 0;
		}

		const std::set<int> getIndices() const {
			return { graphicsQueue, presentQueue };
		}
	};
	struct SwapchainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};
private:
	SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice adevice, VkSurfaceKHR asurface);
	ActiveQueues getGraphicsQueueFamilyIndex(VkPhysicalDevice device, VkSurfaceKHR surface);
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkDebugReportCallbackEXT callback;
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
	ActiveQueues queues;
	bool debugEnabled;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	size_t currentFrame = 0;
	bool framebufferResized = false;
	VkQueue graphicsQueue, presentQueue;
};

