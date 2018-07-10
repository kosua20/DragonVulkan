#include "Renderer.hpp"
#include "VulkanUtilities.hpp"
#include <iostream>
#include <array>

const int MAX_FRAMES_IN_FLIGHT = 2;

struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;
	
	// Binding location, rate of input.
	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}
	// Attribute layout.
	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};
		// Position
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);
		// Color
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);
		return attributeDescriptions;
	}
	
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};


const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0
};

Renderer::Renderer(VkInstance & anInstance, VkSurfaceKHR & aSurface)
{
	_instance = anInstance;
	_surface = aSurface;

}


Renderer::~Renderer()
{
}


int Renderer::init(const int width, const int height){
	_size = glm::vec2(width, height);
	
	/// Setup physical device (GPU).
	VulkanUtilities::createPhysicalDevice(_instance, _surface, _physicalDevice);

	/// Setup logical device.
	// Queue setup.
	VulkanUtilities::ActiveQueues queues = VulkanUtilities::getGraphicsQueueFamilyIndex(_physicalDevice, _surface);
	std::set<int> uniqueQueueFamilies = queues.getIndices();
	// Device features we want.
	VkPhysicalDeviceFeatures deviceFeatures = {};

	/// Create the logical device.
	VulkanUtilities::createDevice(_physicalDevice, uniqueQueueFamilies, deviceFeatures, _device);
	/// Get references to the queues.
	vkGetDeviceQueue(_device, queues.graphicsQueue, 0, &_graphicsQueue);
	vkGetDeviceQueue(_device, queues.presentQueue, 0, &_presentQueue);

	/// Command pool.
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queues.graphicsQueue;
	poolInfo.flags = 0; // Optional
	if(vkCreateCommandPool(_device, &poolInfo, nullptr, &_commandPool) != VK_SUCCESS) {
		std::cerr << "Unable to create command pool." << std::endl;
		return 5;
	}

	/// Create the swap chain.
	_size[0] = width;
	_size[1] = height;

	VulkanUtilities::ActiveQueues queues = VulkanUtilities::getGraphicsQueueFamilyIndex(_physicalDevice, _surface);
	_swapchainParams = VulkanUtilities::generateSwapchainParameters(_physicalDevice, _surface, width, height);
	VulkanUtilities::createSwapchain(_swapchainParams, _surface, _device, queues, _swapchain);

	/// Render pass.
	createMainRenderpass();

	/// Pipeline.
	createPipeline();

	fillSwapchain(_mainRenderPass);
	
	/// Vertex buffer.
	// TODO: bundle all of this away.
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
	// Use a staging buffer as an intermediate.
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VulkanUtilities::createBuffer(_physicalDevice, _device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	// Fill it.
	void* data;
	vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t) bufferSize);
	vkUnmapMemory(_device, stagingBufferMemory);
	// Create the destination buffer.
	VulkanUtilities::createBuffer(_physicalDevice, _device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vertexBuffer, _vertexBufferMemory);
	// Copy from the staging buffer to the final.
	// TODO: use specific command pool.
	VulkanUtilities::copyBuffer(stagingBuffer, _vertexBuffer, bufferSize, _device, _commandPool, _graphicsQueue);
	vkDestroyBuffer(_device, stagingBuffer, nullptr);
	vkFreeMemory(_device, stagingBufferMemory, nullptr);
	
	/// Index buffer.
	bufferSize = sizeof(indices[0]) * indices.size();
	// Create and fill the staging buffer.
	VulkanUtilities::createBuffer(_physicalDevice, _device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t) bufferSize);
	vkUnmapMemory(_device, stagingBufferMemory);
	// Create and copy final buffer.
	VulkanUtilities::createBuffer(_physicalDevice, _device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _indexBuffer, _indexBufferMemory);
	VulkanUtilities::copyBuffer(stagingBuffer, _indexBuffer, bufferSize, _device, _commandPool, _graphicsQueue);
	vkDestroyBuffer(_device, stagingBuffer, nullptr);
	vkFreeMemory(_device, stagingBufferMemory, nullptr);
	
	// Command buffers.
	generateCommandBuffers();
	
	/// Semaphores and fences.
	_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(_device, &fenceInfo, nullptr, &_inFlightFences[i]) != VK_SUCCESS) {
			std::cerr << "Unable to create semaphores and fences." << std::endl;
			return 3;
		}
	}
	return 0;
}

VkResult Renderer::draw(){
	vkWaitForFences(_device, 1, &_inFlightFences[_currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(_device, 1, &_inFlightFences[_currentFrame]);

	// Acquire image from swap chain.
	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(_device, _swapchain, std::numeric_limits<uint64_t>::max(), _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);
	if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		return result;
	}
	// Submit command buffer.
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkSemaphore waitSemaphores[] = { _imageAvailableSemaphores[_currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &_commandBuffers[imageIndex];
	VkSemaphore signalSemaphores[] = { _renderFinishedSemaphores[_currentFrame] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	if(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _inFlightFences[_currentFrame]) != VK_SUCCESS) {
		std::cerr << "Unable to submit commands." << std::endl;
	}
	// Present on swap chain.
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	VkSwapchainKHR swapChains[] = { _swapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr; // Optional
	result = vkQueuePresentKHR(_presentQueue, &presentInfo);
	if(result != VK_SUCCESS) {
		return result;
	}
	_currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	return result;
}

int Renderer::fillSwapchain(VkRenderPass & renderPass){
	// Retrieve images in the swap chain.
	vkGetSwapchainImagesKHR(_device, _swapchain, &_swapchainParams.count, nullptr);
	_swapchainImages.resize(_swapchainParams.count);
	vkGetSwapchainImagesKHR(_device, _swapchain, &_swapchainParams.count, _swapchainImages.data());
	// Create views for each image.
	_swapchainImageViews.resize(_swapchainImages.size());
	for(size_t i = 0; i < _swapchainImages.size(); i++) {
		VkImageViewCreateInfo createImageViewInfo = {};
		createImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createImageViewInfo.image = _swapchainImages[i];
		createImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createImageViewInfo.format = _swapchainParams.surface.format;
		createImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createImageViewInfo.subresourceRange.baseMipLevel = 0;
		createImageViewInfo.subresourceRange.levelCount = 1;
		createImageViewInfo.subresourceRange.baseArrayLayer = 0;
		createImageViewInfo.subresourceRange.layerCount = 1;
		if(vkCreateImageView(_device, &createImageViewInfo, nullptr, &_swapchainImageViews[i]) != VK_SUCCESS) {
			std::cerr << "Unable to create image view." << std::endl;
			return 4;
		}
	}
	// Swap framebuffers.
	_swapchainFramebuffers.resize(_swapchainImageViews.size());
	for(size_t i = 0; i < _swapchainImageViews.size(); i++){
		VkImageView attachments[] = {
			_swapchainImageViews[i]
		};
		
		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = _swapchainParams.extent.width;
		framebufferInfo.height = _swapchainParams.extent.height;
		framebufferInfo.layers = 1;
		if(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_swapchainFramebuffers[i]) != VK_SUCCESS) {
			std::cerr << "Unable to create swap framebuffers." << std::endl;
			return 4;
		}
	}
	return 0;
}

int Renderer::createMainRenderpass(){
	/// Render pass.
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = _swapchainParams.surface.format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef; // linked to the layout in fragment shader.
	
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	
	// Dependencies.
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;
	
	if(vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_mainRenderPass) != VK_SUCCESS) {
		std::cerr << "Unable to create render pass." << std::endl;
		return 3;
	}
	return 0;
}

int Renderer::createPipeline(){
	
	/// Shaders.
	auto vertShaderCode = VulkanUtilities::readFile("resources/shaders/vert.spv");
	auto fragShaderCode = VulkanUtilities::readFile("resources/shaders/frag.spv");
	VkShaderModule vertShaderModule = VulkanUtilities::createShaderModule(_device, vertShaderCode);
	VkShaderModule fragShaderModule = VulkanUtilities::createShaderModule(_device, fragShaderCode);
	// Vertex shader module.
	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";
	// Fragment shader module.
	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	// Vertex input format.
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	// Binding and attributes to use.
	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
	
	// Geometry assembly.
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;
	// Viewport and scissor.
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)_swapchainParams.extent.width;
	viewport.height = (float)_swapchainParams.extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = _swapchainParams.extent;
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;
	// Rasterization.
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	// Multisampling (none).
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	// Depth/stencil (none).
	// Blending (none).
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	// Dynamic state (none).
	
	// Finally, the layout.
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	// No uniforms for now.
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
	if(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout) != VK_SUCCESS) {
		std::cerr << "Unable to create pipeline layout." << std::endl;
		return 3;
	}
	// And the pipeline.
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr; // Optional
	pipelineInfo.layout = _pipelineLayout;
	pipelineInfo.renderPass = _mainRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional
	if(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline) != VK_SUCCESS) {
		std::cerr << "Unable to create graphics pipeline." << std::endl;
		return 3;
	}


	vkDestroyShaderModule(_device, fragShaderModule, nullptr);
	vkDestroyShaderModule(_device, vertShaderModule, nullptr);

	return 0;
}

int Renderer::generateCommandBuffers(){
	_commandBuffers.resize(_swapchainFramebuffers.size());
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = _commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(_commandBuffers.size());
	if(vkAllocateCommandBuffers(_device, &allocInfo, _commandBuffers.data()) != VK_SUCCESS) {
		std::cerr << "Unable to create command buffers." << std::endl;
		return 5;
	}
	for(size_t i = 0; i < _commandBuffers.size(); i++) {
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		beginInfo.pInheritanceInfo = nullptr; // Optional
		
		if(vkBeginCommandBuffer(_commandBuffers[i], &beginInfo) != VK_SUCCESS) {
			std::cerr << "Unable to begin recording command buffer." << std::endl;
			return 5;
		}
		// Render pass commands.
		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = _mainRenderPass;
		renderPassInfo.framebuffer = _swapchainFramebuffers[i];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = _swapchainParams.extent;
		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;
		vkCmdBeginRenderPass(_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		// Bind and draw.
		vkCmdBindPipeline(_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
		VkBuffer vertexBuffers[] = {_vertexBuffer};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(_commandBuffers[i], 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(_commandBuffers[i], _indexBuffer, 0, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(_commandBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
		
		vkCmdEndRenderPass(_commandBuffers[i]);
		
		if(vkEndCommandBuffer(_commandBuffers[i]) != VK_SUCCESS) {
			std::cerr << "Unable to end recording command buffer." << std::endl;
			return 5;
		}
	}
	return 0;
}

int Renderer::createSwapchain(const int width, const int height){
	_size[0] = width;
	_size[1] = height;
	
	VulkanUtilities::ActiveQueues queues = VulkanUtilities::getGraphicsQueueFamilyIndex(_physicalDevice, _surface);
	_swapchainParams = VulkanUtilities::generateSwapchainParameters(_physicalDevice, _surface, width, height);
	VulkanUtilities::createSwapchain(_swapchainParams, _surface, _device, queues, _swapchain);

	/// Render pass.
	createMainRenderpass();

	

	/// Pipeline.
	createPipeline();

	
	
	fillSwapchain(_mainRenderPass);
	
	
	return 0;
}

int Renderer::resize(const int width, const int height){
	if(width == _size[0] && height == _size[1]){
		return 0;
	}
	vkDeviceWaitIdle(_device);
	cleanupSwapChain();
	createSwapchain(width, height);
	// Command buffers.
	generateCommandBuffers();
	return 0;
}

void Renderer::cleanup(){
	
	vkDeviceWaitIdle(_device);
	cleanupSwapChain();
	
	vkDestroyBuffer(_device, _vertexBuffer, nullptr);
	vkFreeMemory(_device, _vertexBufferMemory, nullptr);
	vkDestroyBuffer(_device, _indexBuffer, nullptr);
	vkFreeMemory(_device, _indexBufferMemory, nullptr);
	
	for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(_device, _renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(_device, _imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(_device, _inFlightFences[i], nullptr);
	}
	vkDestroyCommandPool(_device, _commandPool, nullptr);
	vkDestroyDevice(_device, nullptr);
	
}

void Renderer::cleanupSwapChain() {
	for(size_t i = 0; i < _swapchainFramebuffers.size(); i++) {
		vkDestroyFramebuffer(_device, _swapchainFramebuffers[i], nullptr);
	}
	vkFreeCommandBuffers(_device, _commandPool, static_cast<uint32_t>(_commandBuffers.size()), _commandBuffers.data());
	vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
	vkDestroyRenderPass(_device, _mainRenderPass, nullptr);
	for(size_t i = 0; i < _swapchainImageViews.size(); i++) {
		vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
	}
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}
