#pragma once
#include "Object.hpp"
#include "Skybox.hpp"
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

	void encode(VkCommandBuffer & commandBuffer, VkQueue & graphicsQueue, VkSemaphore & imageAvailableSemaphore, VkRenderPassBeginInfo & finalRenderPassInfos, VkSubmitInfo & submitInfo, const uint32_t index);
	
	void update(const double deltaTime);
	
	void resize(VkRenderPass & finalRenderPass, const int width, const int height);
	
	void clean();
	
private:
	
	void createPipelines(const VkRenderPass & finalRenderPass);
	void createObjectsDescriptors();
	void createObjectsPipeline(const VkRenderPass & finalRenderPass);
	void createSkyboxDescriptors();
	void createSkyboxPipeline(const VkRenderPass & finalRenderPass);
	void createShadowDescriptors();
	void createShadowPipeline(const VkRenderPass & shadowRenderPass);
	
	struct OffscreenPass {
		glm::vec2 size = glm::vec2(1024.0f, 1024.0f);
		VkFramebuffer frameBuffer;
		VkImage depthImage;
		VkDeviceMemory depthMemory;
		VkImageView depthView;
		VkRenderPass renderPass;
		VkSampler depthSampler;
		VkDescriptorImageInfo descriptor;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkSemaphore semaphore = VK_NULL_HANDLE;
	} _shadowPass;
	VkPipelineLayout _shadowPipelineLayout;
	VkPipeline _shadowPipeline;
	VkDescriptorSetLayout _shadowDescriptorSetLayout;
	
	VkDevice _device;
	
	VkPipelineLayout _objectPipelineLayout;
	VkPipeline _objectPipeline;
	VkDescriptorSetLayout _objectDescriptorSetLayout;
	
	VkPipelineLayout _skyboxPipelineLayout;
	VkPipeline _skyboxPipeline;
	VkDescriptorSetLayout _skyboxDescriptorSetLayout;
	
	
	
	std::vector<VkDescriptorPool> _descriptorPools;
	
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

