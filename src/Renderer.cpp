#include "Renderer.hpp"
#include "VulkanUtilities.hpp"
#include "resources/Resources.hpp"

#include <array>

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

Renderer::~Renderer(){
}

Renderer::Renderer(Swapchain & swapchain, const int width, const int height){
	
	const auto & physicalDevice = swapchain.physicalDevice;
	const auto & commandPool = swapchain.commandPool;
	const auto & finalRenderPass = swapchain.finalRenderPass;
	const auto & graphicsQueue = swapchain.graphicsQueue;
	const uint32_t count = swapchain.count;
	_device = swapchain.device;
	 
	_objects.emplace_back("dragon");
	_objects.emplace_back("suzanne");
	_objects.emplace_back("plane");
	_objects.emplace_back("cubemap");

	_size = glm::vec2(width, height);
	
	// Descriptor layout.
	// Uniform binding.
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;// binding in 0.
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;// no texture
	// Image+sampler binding.
	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	// Create the layout (== defining a struct)
	std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();
	if (vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &_descriptorSetLayout) != VK_SUCCESS) {
		std::cerr << "Unable to create uniform descriptor." << std::endl;
	}
	
	/// Pipeline.
	createPipeline(finalRenderPass);
	
	// Create sampler.
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;
	if (vkCreateSampler(_device, &samplerInfo, nullptr, &_textureSampler) != VK_SUCCESS) {
		std::cerr << "Unable to create a sampler." << std::endl;
	}
	// Objects setup.
	for(auto & object : _objects){
		object.upload(physicalDevice, _device, commandPool, graphicsQueue);
	}
	
	/// Uniform buffers.
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	_uniformBuffers.resize(count);
	_uniformBuffersMemory.resize(count);
	for (size_t i = 0; i < count; i++) {
		VulkanUtilities::createBuffer(physicalDevice, _device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _uniformBuffers[i], _uniformBuffersMemory[i]);
	}
	// Create descriptor pool.
	// 2 pools: one for uniform, one for image+sampler.
	std::array<VkDescriptorPoolSize, 2> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(count);
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(count);
	VkDescriptorPoolCreateInfo descPoolInfo = {};
	descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	descPoolInfo.pPoolSizes = poolSizes.data();
	descPoolInfo.maxSets = static_cast<uint32_t>(count);
	
	if (vkCreateDescriptorPool(_device, &descPoolInfo, nullptr, &_descriptorPool) != VK_SUCCESS) {
		std::cerr << "Unable to create descriptor pool." << std::endl;
	}
	// Create descriptors sets.
	std::vector<VkDescriptorSetLayout> layouts(count, _descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = _descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(count);
	allocInfo.pSetLayouts = layouts.data();
	_descriptorSets.resize(count);
	if (vkAllocateDescriptorSets(_device, &allocInfo, &_descriptorSets[0]) != VK_SUCCESS) {
		std::cerr << "Unable to create descriptor sets." << std::endl;
	}
	for (size_t i = 0; i < count; i++) {
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = _uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = _objects[0]._textureImageView;
		imageInfo.sampler = _textureSampler;
		
		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = _descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;
		descriptorWrites[0].pImageInfo = nullptr; // Optional
		descriptorWrites[0].pTexelBufferView = nullptr; // Optional
		
		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = _descriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;
		
		vkUpdateDescriptorSets(_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

void Renderer::createPipeline(const VkRenderPass & finalRenderPass){
	// This is independent from the RTs.
	/// Shaders.
	VkShaderModule vertShaderModule = VulkanUtilities::createShaderModule(_device, "resources/shaders/vert.spv");
	VkShaderModule fragShaderModule = VulkanUtilities::createShaderModule(_device, "resources/shaders/frag.spv");
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
	viewport.width = (float)_size[0];
	viewport.height = (float)_size[1];
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = {(uint32_t)_size[0], (uint32_t)_size[1]};
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
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	// Multisampling (none).
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	// Depth/stencil (none).
	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Default
	depthStencil.maxDepthBounds = 1.0f; // Default
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {}; // Optional
	depthStencil.back = {}; // Optional
	
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
	// Uniforms setup.
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &_descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
	if(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout) != VK_SUCCESS) {
		std::cerr << "Unable to create pipeline layout." << std::endl;
		return;
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
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr; // Optional
	pipelineInfo.layout = _pipelineLayout;
	pipelineInfo.renderPass = finalRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional
	if(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline) != VK_SUCCESS) {
		std::cerr << "Unable to create graphics pipeline." << std::endl;
		
	}
	
	vkDestroyShaderModule(_device, fragShaderModule, nullptr);
	vkDestroyShaderModule(_device, vertShaderModule, nullptr);
}

void Renderer::encode(VkCommandBuffer & commandBuffer, VkRenderPassBeginInfo & finalRenderPassInfos, const uint32_t index){
	
	// Update uniforms.
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
	UniformBufferObject ubo = {};
	ubo.model = glm::mat4(1.0f);
	ubo.view = _camera.view();
	ubo.proj = _camera.projection();
	ubo.proj[1][1] *= -1; // Flip compared to OpenGL.
	// Send data.
	void* data;
	vkMapMemory(_device, _uniformBuffersMemory[index], 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(_device, _uniformBuffersMemory[index]);
	
	// Final pass.
	vkCmdBeginRenderPass(commandBuffer, &finalRenderPassInfos, VK_SUBPASS_CONTENTS_INLINE);
	// Bind and draw.
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
	for(auto & object : _objects){
		VkBuffer vertexBuffers[] = {object._vertexBuffer};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, object._indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		// Uniform descriptor sets.
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &_descriptorSets[index], 0, nullptr);
		vkCmdDrawIndexed(commandBuffer, object._count, 1, 0, 0, 0);
	}
	
	vkCmdEndRenderPass(commandBuffer);
}

void Renderer::update(const double deltaTime) {
	_camera.update();
	_camera.physics(deltaTime);
}

void Renderer::resize(VkRenderPass & finalRenderPass, const int width, const int height){
	if(width == _size[0] && height == _size[1]){
		return;
	}
	_camera.ratio(float(width)/float(height));
	
	_size[0] = width; _size[1] = height;
	/// Recreate Pipeline.
	vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
	createPipeline(finalRenderPass);
}

void Renderer::clean(){
	vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
	vkDestroySampler(_device, _textureSampler, nullptr);
	vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, nullptr);
	for (size_t i = 0; i < _uniformBuffers.size(); i++) {
		vkDestroyBuffer(_device, _uniformBuffers[i], nullptr);
		vkFreeMemory(_device, _uniformBuffersMemory[i], nullptr);
	}
	for(auto & object : _objects){
		object.clean(_device);
	}
}

