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

const int WIDTH = 800;
const int HEIGHT = 600;



/// Validation layers shenanigans.



#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif







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
	/// Init Vulkan.
	Renderer renderer;
	renderer.init(window, enableValidationLayers);
	
	
	/// Main loop.
	while(!glfwWindowShouldClose(window)){
		glfwPollEvents();
		/// Draw frame.
		renderer.draw(window);
	}
	

	/// Cleanup.
	renderer.cleanup();
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}