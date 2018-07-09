#pragma once

#include <vulkan/vulkan.hpp>
#include <string>
#include <vector>
#include <set>

class VulkanUtilities {
public:

	/// Shaders.
	static std::vector<char> readFile(const std::string& filename);
	static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);

	/// Extensions.
	static bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	static std::vector<const char*> getRequiredInstanceExtensions(const bool enableValidationLayers);
	
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
	
	/// Device validation.
	static bool isDeviceSuitable(VkPhysicalDevice adevice, VkSurfaceKHR asurface);
	
	/// Swap chain.
	struct SwapchainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};
	static SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice adevice, VkSurfaceKHR asurface);
	static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const int width, const int height);
	static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes);
	
	/// Debug.
	static bool checkValidationLayerSupport();
	static void cleanupDebug(VkInstance & instance);
	/// General setup.
	static int createInstance(const std::string & name, const bool debugEnabled, VkInstance & instance);

	static int createPhysicalDevice(VkInstance & instance, VkSurfaceKHR & surface, VkPhysicalDevice & physicalDevice);

	static int createDevice(VkPhysicalDevice & physicalDevice, std::set<int> & queuesIds, VkPhysicalDeviceFeatures & features, VkDevice & device);


	static bool layersEnabled;
	static VkDebugReportCallbackEXT callback;
};

