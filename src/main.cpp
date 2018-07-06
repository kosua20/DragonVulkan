#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>



const int WIDTH = 800;
const int HEIGHT = 600;

int main() {

	/// Init GLFW3.
	if(!glfwInit()){
		std::cerr << "Unable to initialize GLFW." << std::endl;
		return 1;
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
	// Default Vulkan has no notion of surface/window. GLFW provide an implementation of the corresponding KHR extension.
	// We have to tell Vulkan it exists.
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	// Get GLFW exposed extensions.
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	// Add them to the instance infos.
	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions;
	// Validation layers.
	createInfo.enabledLayerCount = 0;

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
		std::cerr << "Unable to create a Vulkan instance." << std::endl;
		return 3;
	}

	/// Main loop.
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}

	/// Cleanup.
	vkDestroyInstance(instance, nullptr);
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}