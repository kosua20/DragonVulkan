//
//  ShadowPass.hpp
//  DragonVulkan
//
//  Created by Simon Rodriguez on 15/07/2018.
//  Copyright © 2018 Simon Rodriguez. All rights reserved.
//

#ifndef ShadowPass_hpp
#define ShadowPass_hpp

#include "common.hpp"
#include "Object.hpp"

class ShadowPass {
	
public:
	
	ShadowPass(const int width, const int height);
	
	void init(const VkPhysicalDevice & physicalDevice,const VkDevice & device, const VkCommandPool & commandPool);
	
	void createDescriptors(const VkDevice & device);
	
	void createPipeline(const VkDevice & device);
	
	void generateCommandBuffer(const std::vector<Object> & objects);
	
	void clean(const VkDevice & device);
	
	void resetSemaphore(const VkDevice & device);
	
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
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkDescriptorSetLayout descriptorSetLayout;
	
};



#endif /* ShadowPass_hpp */
