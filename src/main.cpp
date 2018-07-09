#ifdef _WIN32
#define NOMINMAX
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <fstream>
#include <string>

#include "Renderer.hpp"

const int WIDTH = 400;
const int HEIGHT = 300;

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

/// GLFW callbacks.
static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	auto renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	renderer->resize(width, height);
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
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	// Create window.
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Dragon Vulkan", nullptr, nullptr);
	if(!window){
		std::cerr << "Unable to create GLFW window." << std::endl;
		return 2;
	}
	//Get window effective size.
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	// Debug setup.
	bool debugEnabled = enableValidationLayers;
	// Check if the validation layers are needed and available.
	if(enableValidationLayers && !VulkanUtilities::checkValidationLayerSupport()){
		std::cerr << "Validation layers required and unavailable." << std::endl;
		debugEnabled = false;
	}

	/// Vulkan instance creation.
	VkInstance instance;
	VulkanUtilities::createInstance("Dragon Vulkan", debugEnabled, instance);
	
	/// Surface window setup.
	VkSurfaceKHR surface;
	if(glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
		std::cerr << "Unable to create the surface." << std::endl;
		return 2;
	}
	
	/// Create the renderer.	
	Renderer renderer(instance, surface);
	renderer.init(width, height);
	
	/// Register callbacks.
	glfwSetWindowUserPointer(window, &renderer);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	
	/// Main loop.
	while(!glfwWindowShouldClose(window)){
		glfwPollEvents();
		/// Draw frame.
		const VkResult result = renderer.draw();
		if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR){
			int width = 0, height = 0;
			while(width == 0 || height == 0) {
				glfwGetFramebufferSize(window, &width, &height);
				glfwWaitEvents();
			}
			renderer.resize(width, height);
		}
	}

	/// Cleanup.
	renderer.cleanup();
	// Clean up instance and surface.
	VulkanUtilities::cleanupDebug(instance);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
	// Clean up GLFW.
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
