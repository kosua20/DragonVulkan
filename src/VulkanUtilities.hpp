#pragma once

#include "common.hpp"
#include <set>

class VulkanUtilities {
public:

	/// Shaders.
	static VkShaderModule createShaderModule(VkDevice device, const std::string& path);

	/// Queues.
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
	static ActiveQueues getGraphicsQueueFamilyIndex(VkPhysicalDevice device, VkSurfaceKHR surface);
	
	/// Swap chain.
	struct SwapchainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};
	struct SwapchainParameters {
		VulkanUtilities::SwapchainSupportDetails support;
		VkExtent2D extent;
		VkSurfaceFormatKHR surface;
		VkPresentModeKHR mode;
		uint32_t count;
	};
	static SwapchainParameters generateSwapchainParameters(VkPhysicalDevice & physicalDevice, VkSurfaceKHR & surface, const int width, const int height);
	
	/// Debug.
	static bool checkValidationLayerSupport();
	static void cleanupDebug(VkInstance & instance);

	/// General setup.
	static int createInstance(const std::string & name, const bool debugEnabled, VkInstance & instance);

	static int createPhysicalDevice(VkInstance & instance, VkSurfaceKHR & surface, VkPhysicalDevice & physicalDevice);

	static int createDevice(VkPhysicalDevice & physicalDevice, std::set<int> & queuesIds, VkPhysicalDeviceFeatures & features, VkDevice & device);

	static int createSwapchain(SwapchainParameters & parameters, VkSurfaceKHR & surface, VkDevice & device, ActiveQueues & queues,VkSwapchainKHR & swapchain);

	static VkCommandBuffer beginOneShotCommandBuffer( const VkDevice & device,  const VkCommandPool & commandPool);
	
	static void endOneShotCommandBuffer(VkCommandBuffer & commandBuffer,  const VkDevice & device,  const VkCommandPool & commandPool,  const VkQueue & queue);
	
	static int createBuffer(const VkPhysicalDevice & physicalDevice, const VkDevice & device, const VkDeviceSize & size, const VkBufferUsageFlags & usage, const VkMemoryPropertyFlags & properties, VkBuffer & buffer, VkDeviceMemory & bufferMemory);
	
	static int createImage(const VkPhysicalDevice & physicalDevice, const VkDevice & device, const uint32_t & width, const uint32_t & height, const VkFormat & format, const VkImageTiling & tiling, const VkImageUsageFlags & usage, const VkMemoryPropertyFlags & properties, VkImage & image, VkDeviceMemory & imageMemory);
	
	static void copyBuffer(const VkBuffer & srcBuffer, const VkBuffer & dstBuffer, const VkDeviceSize & size, const VkDevice & device, const VkCommandPool & commandPool, const VkQueue & queue);
	
	static void copyBufferToImage(const VkBuffer & srcBuffer, const VkImage & dstImage, const uint32_t & width, const uint32_t & height, const VkDevice & device, const VkCommandPool & commandPool, const VkQueue & queue);
	
	static void transitionImageLayout(const VkDevice & device, const VkCommandPool & commandPool, const VkQueue & queue, VkImage & image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	
	static VkImageView createImageView(const VkDevice & device, const VkImage & image, const VkFormat format, const VkImageAspectFlags aspectFlags);
	
	static VkFormat findDepthFormat(const VkPhysicalDevice & physicalDevice);
	
	static bool hasStencilComponent(VkFormat format);
	
private:
	/// Device validation.
	static bool isDeviceSuitable(VkPhysicalDevice adevice, VkSurfaceKHR asurface);
	static uint32_t findMemoryType(const uint32_t typeFilter, const VkMemoryPropertyFlags & properties, const VkPhysicalDevice & physicalDevice);
	static VkFormat findSupportedFormat(const VkPhysicalDevice & physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	
	static bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	static std::vector<const char*> getRequiredInstanceExtensions(const bool enableValidationLayers);
	
	static SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice adevice, VkSurfaceKHR asurface);
	static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const int width, const int height);
	static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes);
	
	static bool layersEnabled;
	static VkDebugReportCallbackEXT callback;
};

