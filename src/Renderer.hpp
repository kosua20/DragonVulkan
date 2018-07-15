#pragma once
#include "Object.hpp"
#include "Swapchain.hpp"

#include "VulkanUtilities.hpp"
#include "input/ControllableCamera.hpp"

#include "common.hpp"
#include <chrono>
struct GLFWwindow;

class Renderer
{
public:

	Renderer(Swapchain & swapchain, const int width, const int height);

	~Renderer();

	void encode(VkCommandBuffer & commandBuffer, VkRenderPassBeginInfo & finalRenderPassInfos,  const uint32_t index);
	
	void update(const double deltaTime);
	
	void resize(VkRenderPass & finalRenderPass, const int width, const int height);
	
	void clean();
	
private:
	
	void createPipeline(const VkRenderPass & finalRenderPass);
	
	VkDevice _device;
	
	VkPipelineLayout _pipelineLayout;
	VkPipeline _graphicsPipeline;
	
	VkDescriptorSetLayout _descriptorSetLayout;
	std::vector<VkDescriptorPool> _descriptorPools;
	
	VkSampler _textureSampler;
	
	std::vector<Object> _objects;
	
	std::vector<VkBuffer> _uniformBuffers;
	std::vector<VkDeviceMemory> _uniformBuffersMemory;
	
	glm::vec2 _size = glm::vec2(0.0f,0.0f);
	ControllableCamera _camera;
	double _time = 0.0;
};

