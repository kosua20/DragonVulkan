#ifdef _WIN32
#define NOMINMAX
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <set>
#include <algorithm>

const int WIDTH = 800;
const int HEIGHT = 600;



/// Validation layers shenanigans.

const std::vector<const char*> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

bool checkValidationLayerSupport(){
	// Get available layers.
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
	// Cross check with those we want.
	for(const char* layerName : validationLayers){
		bool layerFound = false;
		for(const auto& layerProperties : availableLayers){
			if(strcmp(layerName, layerProperties.layerName) == 0){
				layerFound = true;
				break;
			}
		}
		if(!layerFound){
			return false;
		}
	}
	return true;
}


/// Extensions shenanigans.

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
	// Get available device extensions.
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
	// Check if the required device extensions are available.
	for(const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}
	return requiredExtensions.empty();
}

std::vector<const char*> getRequiredInstanceExtensions(){
	// Default Vulkan has no notion of surface/window. GLFW provide an implementation of the corresponding KHR extensions.
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	// If the validation layers are enabled, add associated extensions.
	if(enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}
	return extensions;
}


/// Debug callback.

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code,
	const char* layerPrefix, const char* msg, void* userData) {
	std::cerr << "validation layer: " << msg << std::endl;
	return VK_FALSE;
}

// The callback registration methid is itself an extension, so we have to query it by hand and wrap it...
VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if(func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pCallback);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

// Same for destruction.
void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if(func != nullptr) {
		func(instance, callback, pAllocator);
	}
}


/// Queue families support.

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

ActiveQueues getGraphicsQueueFamilyIndex(VkPhysicalDevice device, VkSurfaceKHR surface){
	ActiveQueues queues;
	// Get all queues.
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
	// Find a queue with graphics support.
	int i = 0;
	for(const auto& queueFamily : queueFamilies){
		// Check if queue support graphics.
		if(queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			queues.graphicsQueue = i;
		}
		// CHeck if queue support presentation.
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		if(queueFamily.queueCount > 0 && presentSupport) {
			queues.presentQueue = i;
		}
		if(queues.isComplete()){
			break;
		}

		++i;
	}
	return queues;
}


/// Swap chain support.

struct SwapchainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
	SwapchainSupportDetails details;
	// Basic capabilities.
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
	// Supported formats.
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	if(formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}
	// Supported modes.
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
	if(presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}
	return details;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
	if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	} else {
		VkExtent2D actualExtent = { WIDTH, HEIGHT };

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	// If undefined, the surface doesn't care, we pick what we want.
	// ie RGBA8 with a sRGB display.
	if(availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
		return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}
	// Else, is our preferred choice available?
	for(const auto& availableFormat : availableFormats) {
		if(availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}
	// Else just take what is given.
	return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
	// By default only FIFO (~V-sync mode) is available.
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	for(const auto& availablePresentMode : availablePresentModes) {
		if(availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			// If available, we directly pick triple buffering.
			return availablePresentMode;
		} else if(availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			bestMode = availablePresentMode;
		}
	}

	return bestMode;
}


/// Device validation.

bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface){
	bool extensionsSupported = checkDeviceExtensionSupport(device);
	bool isComplete = getGraphicsQueueFamilyIndex(device, surface).isComplete();
	bool swapChainAdequate = false;
	if(extensionsSupported) {
		SwapchainSupportDetails swapChainSupport = querySwapchainSupport(device, surface);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}
	return extensionsSupported && isComplete && swapChainAdequate;
}


/// Entry point.

int main() {

	/// Init GLFW3.
	if(!glfwInit()){
		std::cerr << "Unable to initialize GLFW." << std::endl;
		return 2;
	}
	// Don't create an OpenGL context.
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	// Create window.
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Dragon Vulkan", nullptr, nullptr);
	if(!window){
		std::cerr << "Unable to create GLFW window." << std::endl;
		return 2;
	}
	/// Init Vulkan.
	// Check if the validation layers are needed and available.
	if(enableValidationLayers && !checkValidationLayerSupport()){
		std::cerr << "Validation layers required and unavailable." << std::endl;
		return 3;
	}
	// Create a Vulkan instance.
	VkInstance instance;
	// Creation setup.
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Dragon Vulkan";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;
	VkInstanceCreateInfo createInstanceInfo = {};
	createInstanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInstanceInfo.pApplicationInfo = &appInfo;
	
	// We have to tell Vulkan the extensions we need.
	const std::vector<const char*> extensions = getRequiredInstanceExtensions();
	// Add them to the instance infos.
	createInstanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInstanceInfo.ppEnabledExtensionNames = extensions.data();
	// Validation layers.
	if(enableValidationLayers){
		createInstanceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInstanceInfo.ppEnabledLayerNames = validationLayers.data();
	} else {
		createInstanceInfo.enabledLayerCount = 0;
	}

	if(vkCreateInstance(&createInstanceInfo, nullptr, &instance) != VK_SUCCESS){
		std::cerr << "Unable to create a Vulkan instance." << std::endl;
		return 3;
	}
	/// Debug callback creation.
	VkDebugReportCallbackEXT callback;
	if(enableValidationLayers){
		VkDebugReportCallbackCreateInfoEXT createCallbackInfo = {};
		createCallbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createCallbackInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		createCallbackInfo.pfnCallback = debugCallback;
		if(CreateDebugReportCallbackEXT(instance, &createCallbackInfo, nullptr, &callback) != VK_SUCCESS) {
			std::cerr << "Unable to register the callback." << std::endl;
			return 3;
		}
	}
	/// SUrface window setup.
	VkSurfaceKHR surface;
	if(glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
		std::cerr << "Unable to create the surface." << std::endl;
		return 2;
	}

	/// Setup physical device (GPU).
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if(deviceCount == 0){
		std::cerr << "No Vulkan GPU available." << std::endl;
		return 3;
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
	// Check which one is ok for our requirements.
	for(const auto& device : devices) {
		if(isDeviceSuitable(device, surface)) {
			physicalDevice = device;
			break;
		}
	}

	if(physicalDevice == VK_NULL_HANDLE) {
		std::cerr << "No GPU satisifies the requirements." << std::endl;
		return 3;
	}

	/// Setup logical device.
	VkDevice device;
	// Queue setup.
	ActiveQueues queues = getGraphicsQueueFamilyIndex(physicalDevice, surface);
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> uniqueQueueFamilies = queues.getIndices();

	float queuePriority = 1.0f;
	for(int queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Device features we want.
	VkPhysicalDeviceFeatures deviceFeatures = {};
	// Device setup.
	VkDeviceCreateInfo createDeviceInfo = {};
	createDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createDeviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createDeviceInfo.pQueueCreateInfos = queueCreateInfos.data();
	createDeviceInfo.pEnabledFeatures = &deviceFeatures;
	// Extensions.
	createDeviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createDeviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
	// Debug layers.
	if(enableValidationLayers) {
		createDeviceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createDeviceInfo.ppEnabledLayerNames = validationLayers.data();
	} else {
		createDeviceInfo.enabledLayerCount = 0;
	}
	if(vkCreateDevice(physicalDevice, &createDeviceInfo, nullptr, &device) != VK_SUCCESS) {
		std::cerr << "Unable to create logical Vulkan device." << std::endl;
		return 3;
	}
	VkQueue graphicsQueue, presentQueue;
	vkGetDeviceQueue(device, queues.graphicsQueue, 0, &graphicsQueue);
	vkGetDeviceQueue(device, queues.presentQueue, 0, &presentQueue);

	/// Swap chain setup.
	SwapchainSupportDetails swapchainSupport = querySwapchainSupport(physicalDevice, surface);
	VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);
	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
	// Set the number of images in the chain.
	uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
	// maxImageCount = 0 if there is no constraint.
	if(swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
		imageCount = swapchainSupport.capabilities.maxImageCount;
	}
	VkSwapchainCreateInfoKHR createSwapInfo = {};
	createSwapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createSwapInfo.surface = surface;
	createSwapInfo.minImageCount = imageCount;
	createSwapInfo.imageFormat = surfaceFormat.format;
	createSwapInfo.imageColorSpace = surfaceFormat.colorSpace;
	createSwapInfo.imageExtent = extent;
	createSwapInfo.imageArrayLayers = 1;
	createSwapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; 
	//VK_IMAGE_USAGE_TRANSFER_DST_BIT if not rendering directly to it.
	// Establish a link with both queues, handling the case where they are the same.
	uint32_t queueFamilyIndices[] = { (uint32_t)(queues.graphicsQueue), (uint32_t)(queues.presentQueue) };
	if(queues.graphicsQueue != queues.presentQueue){
		createSwapInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createSwapInfo.queueFamilyIndexCount = 2;
		createSwapInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		createSwapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createSwapInfo.queueFamilyIndexCount = 0; // Optional
		createSwapInfo.pQueueFamilyIndices = nullptr; // Optional
	}
	createSwapInfo.preTransform = swapchainSupport.capabilities.currentTransform;
	createSwapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createSwapInfo.presentMode = presentMode;
	createSwapInfo.clipped = VK_TRUE;
	createSwapInfo.oldSwapchain = VK_NULL_HANDLE;
	VkSwapchainKHR swapChain;
	if(vkCreateSwapchainKHR(device, &createSwapInfo, nullptr, &swapChain) != VK_SUCCESS) {
		std::cerr << "Unable to create swap chain." << std::endl;
		return 4;
	}
	// Retrieve images in the swap chain.
	std::vector<VkImage> swapChainImages;
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
	VkFormat swapChainImageFormat = surfaceFormat.format;
	VkExtent2D swapChainExtent = extent;
	

	/// Main loop.
	while(!glfwWindowShouldClose(window)){
		glfwPollEvents();
	}

	/// Cleanup.
	vkDestroySwapchainKHR(device, swapChain, nullptr);
	vkDestroyDevice(device, nullptr);
	if(enableValidationLayers) {
		DestroyDebugReportCallbackEXT(instance, callback, nullptr);
	}
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}