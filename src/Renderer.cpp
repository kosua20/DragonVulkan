#include "Renderer.hpp"
#include "VulkanUtilities.hpp"
#include "resources/Resources.hpp"

#include <array>



Renderer::~Renderer(){
}

Renderer::Renderer(Swapchain & swapchain, const int width, const int height) : _skybox("cubemap"){
	
	const auto & physicalDevice = swapchain.physicalDevice;
	const auto & commandPool = swapchain.commandPool;
	const auto & finalRenderPass = swapchain.finalRenderPass;
	const auto & graphicsQueue = swapchain.graphicsQueue;
	const uint32_t count = swapchain.count;
	_device = swapchain.device;
	_worldLightDir = glm::normalize(glm::vec4(1.0f,1.0f,1.0f,0.0f));
	
	_objects.emplace_back("dragon", 64);
	_objects.back().infos.model = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f,0.0f,-0.5f)), glm::vec3(1.2f));
	_objects.emplace_back("suzanne", 8);
	_objects.back().infos.model = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.5, 0.0, 0.5)), glm::vec3(0.65f));
	_objects.emplace_back("plane", 32);
	_objects.back().infos.model = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0,-0.8,0.0)), glm::vec3(2.75f));
	_skybox.infos.model = glm::scale(glm::mat4(1.0f), glm::vec3(15.0f));
	
	_size = glm::vec2(width, height);
	
	// Create sampler.
	_textureSampler = VulkanUtilities::createSampler(_device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
	
	// TODO: object classes could create their descriptor layout.
	createObjectsDescriptors();
	createSkyboxDescriptors();
	
	/// Pipeline.
	createPipelines(finalRenderPass);
	
	// Objects setup.
	for(auto & object : _objects){
		object.upload(physicalDevice, _device, commandPool, graphicsQueue);
	}
	_skybox.upload(physicalDevice, _device, commandPool, graphicsQueue);
	
	/// Uniform buffers.
	VkDeviceSize bufferSize = VulkanUtilities::nextOffset(sizeof(CameraInfos)) + sizeof(LightInfos);
	_uniformBuffers.resize(count);
	_uniformBuffersMemory.resize(count);
	for (size_t i = 0; i < count; i++) {
		VulkanUtilities::createBuffer(physicalDevice, _device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _uniformBuffers[i], _uniformBuffersMemory[i]);
	}
	// Create descriptor pools.
	// 2 pools: one for uniform, one for image+sampler.
	const uint32_t objectsCount = static_cast<uint32_t>(_objects.size())+1;
	std::array<VkDescriptorPoolSize, 2> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = objectsCount*2;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = objectsCount*2;
	VkDescriptorPoolCreateInfo descPoolInfo = {};
	descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	descPoolInfo.pPoolSizes = poolSizes.data();
	descPoolInfo.maxSets = objectsCount;
	_descriptorPools.resize(count);
	for(size_t i = 0; i < count; ++i){
		if (vkCreateDescriptorPool(_device, &descPoolInfo, nullptr, &_descriptorPools[i]) != VK_SUCCESS) {
			std::cerr << "Unable to create descriptor pool." << std::endl;
		}
	}
	
	// Create descriptors sets.
	for(auto & object : _objects){
		object.generateDescriptorSets(_device, _objectDescriptorSetLayout, _descriptorPools, _uniformBuffers);
	}
	_skybox.generateDescriptorSets(_device, _skyboxDescriptorSetLayout, _descriptorPools, _uniformBuffers);
	
}

void Renderer::createObjectsDescriptors(){
	// Descriptor layout for standard objects.
	// Uniform binding.
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;// binding in 0.
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	VkDescriptorSetLayoutBinding uboLayoutLightBinding = {};
	uboLayoutLightBinding.binding = 3;
	uboLayoutLightBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutLightBinding.descriptorCount = 1;
	uboLayoutLightBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	// Image+sampler binding.
	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = &_textureSampler;
	// Image+sampler binding.
	VkDescriptorSetLayoutBinding samplerLayoutNormalBinding = {};
	samplerLayoutNormalBinding.binding = 2;
	samplerLayoutNormalBinding.descriptorCount = 1;
	samplerLayoutNormalBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutNormalBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutNormalBinding.pImmutableSamplers = &_textureSampler;
	// Create the layout (== defining a struct)
	std::array<VkDescriptorSetLayoutBinding, 4> bindings = {uboLayoutBinding, uboLayoutLightBinding, samplerLayoutBinding, samplerLayoutNormalBinding};
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();
	if (vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &_objectDescriptorSetLayout) != VK_SUCCESS) {
		std::cerr << "Unable to create uniform descriptor." << std::endl;
	}
	
}

void Renderer::createSkyboxDescriptors(){
	// Uniform binding.
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;// binding in 0.
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	// Image+sampler binding.
	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = &_textureSampler;
	
	// Create the layout (== defining a struct)
	std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();
	if (vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &_skyboxDescriptorSetLayout) != VK_SUCCESS) {
		std::cerr << "Unable to create uniform descriptor." << std::endl;
	}
}

void Renderer::createPipelines(const VkRenderPass & finalRenderPass){
	createObjectsPipeline(finalRenderPass);
	createSkyboxPipeline(finalRenderPass);
}

void Renderer::createObjectsPipeline(const VkRenderPass & finalRenderPass){
	// This is independent from the RTs.
	/// Shaders.
	VkShaderModule vertShaderModule = VulkanUtilities::createShaderModule(_device, "resources/shaders/compiled/object.vert.spv");
	VkShaderModule fragShaderModule = VulkanUtilities::createShaderModule(_device, "resources/shaders/compiled/object.frag.spv");
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
	// Depth/stencil.
	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;
	
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
	// Push constant (similar to vertex/fragment bytes in metal, small per-frame buffer (128 bytes guaranteed min.)).
	const VkPushConstantRange pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,    .size = (16+1)*4
	};
	// Uniforms setup.
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &_objectDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	if(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_objectPipelineLayout) != VK_SUCCESS) {
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
	pipelineInfo.layout = _objectPipelineLayout;
	pipelineInfo.renderPass = finalRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional
	if(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_objectPipeline) != VK_SUCCESS) {
		std::cerr << "Unable to create graphics pipeline." << std::endl;
		
	}
	
	vkDestroyShaderModule(_device, fragShaderModule, nullptr);
	vkDestroyShaderModule(_device, vertShaderModule, nullptr);
}

void Renderer::createSkyboxPipeline(const VkRenderPass & finalRenderPass){
	// This is independent from the RTs.
	/// Shaders.
	VkShaderModule vertShaderModule = VulkanUtilities::createShaderModule(_device, "resources/shaders/compiled/skybox.vert.spv");
	VkShaderModule fragShaderModule = VulkanUtilities::createShaderModule(_device, "resources/shaders/compiled/skybox.frag.spv");
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
	// Depth/stencil.
	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;
	
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
	// Push constant (similar to vertex/fragment bytes in metal, small per-frame buffer (128 bytes guaranteed min.)).
	const VkPushConstantRange pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,    .size = (16+1)*4
	};
	// Uniforms setup.
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &_skyboxDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	if(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_skyboxPipelineLayout) != VK_SUCCESS) {
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
	pipelineInfo.layout = _skyboxPipelineLayout;
	pipelineInfo.renderPass = finalRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional
	if(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_skyboxPipeline) != VK_SUCCESS) {
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
	CameraInfos ubo = {};
	ubo.view = _camera.view();
	ubo.proj = _camera.projection();
	ubo.proj[1][1] *= -1; // Flip compared to OpenGL.
	LightInfos light = {};
	light.viewSpaceDir = glm::vec3(glm::normalize(ubo.view * _worldLightDir));
	// Send data.
	
	void* data;
	vkMapMemory(_device, _uniformBuffersMemory[index], 0, sizeof(ubo)+sizeof(light), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	memcpy(static_cast<char*>(data) + VulkanUtilities::nextOffset(sizeof(CameraInfos)), &light, sizeof(light));
	vkUnmapMemory(_device, _uniformBuffersMemory[index]);
	
	// Final pass.
	vkCmdBeginRenderPass(commandBuffer, &finalRenderPassInfos, VK_SUBPASS_CONTENTS_INLINE);
	// Bind and draw.
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _objectPipeline);
	for(auto & object : _objects){
		VkBuffer vertexBuffers[] = {object._vertexBuffer};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, object._indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		// Uniform descriptor sets.
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _objectPipelineLayout, 0, 1, &object.descriptorSet(index), 0, nullptr);
		vkCmdPushConstants(commandBuffer, _objectPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, (16+1)*4, &object.infos.model);
		vkCmdDrawIndexed(commandBuffer, object._count, 1, 0, 0, 0);
	}
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipeline);
	VkBuffer vertexBuffers[] = {_skybox._vertexBuffer};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, _skybox._indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	// Uniform descriptor sets.
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipelineLayout, 0, 1, &_skybox.descriptorSet(index), 0, nullptr);
	vkCmdPushConstants(commandBuffer, _skyboxPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, (16+1)*4, &_skybox.infos.model);
	vkCmdDrawIndexed(commandBuffer, _skybox._count, 1, 0, 0, 0);
	
	vkCmdEndRenderPass(commandBuffer);
}

void Renderer::update(const double deltaTime) {
	_time += deltaTime;
	_camera.update();
	_camera.physics(deltaTime);
	//TODO: don't rely on arbitrary indexing.
	_objects[1].infos.model = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.5,0.0,0.5)), float(fmod(_time, 2*M_PI)), glm::vec3(0.0f,1.0f,0.0f)) , glm::vec3(0.65));
}

void Renderer::resize(VkRenderPass & finalRenderPass, const int width, const int height){
	if(width == _size[0] && height == _size[1]){
		return;
	}
	_camera.ratio(float(width)/float(height));
	_size[0] = width; _size[1] = height;
	/// Recreate Pipeline.
	vkDestroyPipeline(_device, _objectPipeline, nullptr);
	vkDestroyPipelineLayout(_device, _objectPipelineLayout, nullptr);
	vkDestroyPipeline(_device, _skyboxPipeline, nullptr);
	vkDestroyPipelineLayout(_device, _skyboxPipelineLayout, nullptr);
	createPipelines(finalRenderPass);
}

void Renderer::clean(){
	vkDestroyPipeline(_device, _objectPipeline, nullptr);
	vkDestroyPipelineLayout(_device, _objectPipelineLayout, nullptr);
	vkDestroyPipeline(_device, _skyboxPipeline, nullptr);
	vkDestroyPipelineLayout(_device, _skyboxPipelineLayout, nullptr);
	vkDestroySampler(_device, _textureSampler, nullptr);
	for(size_t i = 0; i < _descriptorPools.size(); ++i){
		vkDestroyDescriptorPool(_device, _descriptorPools[i], nullptr);
	}
	vkDestroyDescriptorSetLayout(_device, _objectDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(_device, _skyboxDescriptorSetLayout, nullptr);
	for (size_t i = 0; i < _uniformBuffers.size(); i++) {
		vkDestroyBuffer(_device, _uniformBuffers[i], nullptr);
		vkFreeMemory(_device, _uniformBuffersMemory[i], nullptr);
	}
	for(auto & object : _objects){
		object.clean(_device);
	}
	_skybox.clean(_device);
}

