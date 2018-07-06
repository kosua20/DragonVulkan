#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>


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

std::vector<const char*> getRequiredExtensions(){
	// Default Vulkan has no notion of surface/window. GLFW provide an implementation of the corresponding KHR extension.
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
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	
	// We have to tell Vulkan the extensions we need.
	const std::vector<const char*> extensions = getRequiredExtensions();
	// Add them to the instance infos.
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();
	// Validation layers.
	if(enableValidationLayers){
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	} else {
		createInfo.enabledLayerCount = 0;
	}

	if(vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS){
		std::cerr << "Unable to create a Vulkan instance." << std::endl;
		return 3;
	}
	/// Debug callback creation.
	VkDebugReportCallbackEXT callback;
	if(enableValidationLayers){
		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		createInfo.pfnCallback = debugCallback;
		if(CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, &callback) != VK_SUCCESS) {
			std::cerr << "Unable to register the callback." << std::endl;
			return 3;
		}
	}

	/// Main loop.
	while(!glfwWindowShouldClose(window)){
		glfwPollEvents();
	}

	/// Cleanup.
	if(enableValidationLayers) {
		DestroyDebugReportCallbackEXT(instance, callback, nullptr);
	}
	vkDestroyInstance(instance, nullptr);
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}