#pragma once
#include "Object.hpp"
#include "Skybox.hpp"
#include "ShadowPass.hpp"
#include "Swapchain.hpp"

#include "VulkanUtilities.hpp"
#include "input/ControllableCamera.hpp"

#include "common.hpp"
#include <chrono>
//struct GLFWwindow;

class Renderer
{
public:

	Renderer(Swapchain & swapchain, const int width, const int height);

	~Renderer();

	void encode(VkCommandBuffer & commandBuffer, VkQueue & graphicsQueue, VkSemaphore & imageAvailableSemaphore, VkRenderPassBeginInfo & finalRenderPassInfos, VkSubmitInfo & submitInfo, const uint32_t index);
	
	void update(const double deltaTime);
	
	void resize(VkRenderPass & finalRenderPass, const int width, const int height);
	
	void clean();
	
private:
	
	void createPipelines(const VkRenderPass & finalRenderPass);
	void createObjectsDescriptors();
	
	
	ShadowPass _shadowPass;
	
	VkDevice _device;
	
	VkPipelineLayout _objectPipelineLayout;
	VkPipeline _objectPipeline;
	
	VkPipelineLayout _skyboxPipelineLayout;
	VkPipeline _skyboxPipeline;
	
	VkDescriptorPool _descriptorPool;
	
	VkSampler _textureSampler;
	
	std::vector<Object> _objects;
	Skybox _skybox;
	
	std::vector<VkBuffer> _uniformBuffers;
	std::vector<VkDeviceMemory> _uniformBuffersMemory;
	
	VkBuffer _lightUniformBuffer;
	VkDeviceMemory _lightUniformBufferMemory;
	
	glm::vec2 _size = glm::vec2(0.0f,0.0f);
	
	ControllableCamera _camera;
	glm::mat4 _lightViewproj;
	glm::vec4 _worldLightDir;
	double _time = 0.0;
};

