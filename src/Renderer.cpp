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
	glm::mat4 lightProj = glm::ortho(-0.5, 0.5, -0.5, 0.5, 0.2, 5.0);
	lightProj[1][1] *= -1;
	glm::mat4 lightView = glm::lookAt(glm::vec3(2.0,2.0,2.0), glm::vec3(0.0f), glm::vec3(0.0,1.0,0.0));
	_lightViewproj = lightProj * lightView;
	_worldLightDir = glm::normalize(glm::vec4(1.0f,1.0f,1.0f,0.0f));
	
	_objects.emplace_back("dragon", 64);
	_objects.back().infos.model = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f,0.0f,-0.5f)), glm::vec3(1.2f));
	_objects.emplace_back("suzanne", 8);
	_objects.back().infos.model = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.5, 0.0, 0.5)), glm::vec3(0.65f));
	_objects.emplace_back("plane", 32);
	_objects.back().infos.model = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0,-0.8,0.0)), glm::vec3(2.75f));
	_skybox.infos.model = glm::scale(glm::mat4(1.0f), glm::vec3(15.0f));
	
	_size = glm::vec2(width, height);
	// Init shadow pass and framebuffer.
	// For shadow mapping we only need a depth attachment
	VulkanUtilities::createImage(physicalDevice, _device, _shadowPass.size[0], _shadowPass.size[1], VK_FORMAT_D32_SFLOAT , VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, _shadowPass.depthImage, _shadowPass.depthMemory);
	_shadowPass.depthView = VulkanUtilities::createImageView(_device, _shadowPass.depthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, false);
	//VulkanUtilities::transitionImageLayout(device, commandPool, graphicsQueue, _depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, false);
	// Create a sampler for the shadow map.
	_shadowPass.depthSampler = VulkanUtilities::createSampler(_device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	
	VkAttachmentDescription attachmentDescription{};
	attachmentDescription.format = VK_FORMAT_D32_SFLOAT;
	attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store the depth for the next pass.
	attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	// Depth buffer ref.
	VkAttachmentReference depthReference = {};
	depthReference.attachment = 0;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 0; // No color attachment.
	subpass.pDepthStencilAttachment = &depthReference;
	// Dependencies.
	std::array<VkSubpassDependency, 2> dependencies;
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = 0;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	// Creation infos.
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &attachmentDescription;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 0;//static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();
	
	if(vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_shadowPass.renderPass) != VK_SUCCESS) {
		std::cerr << "Unable to create shadow render pass." << std::endl;
	}
	// Create the framebuffer.
	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = _shadowPass.renderPass;
	framebufferInfo.attachmentCount = 1;
	framebufferInfo.pAttachments = &_shadowPass.depthView;
	framebufferInfo.width = _shadowPass.size[0];
	framebufferInfo.height = _shadowPass.size[1];
	framebufferInfo.layers = 1;
	if(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_shadowPass.frameBuffer) != VK_SUCCESS) {
		std::cerr << "Unable to create shadow map framebuffer." << std::endl;
	}
	
	
	
	// Create sampler.
	_textureSampler = VulkanUtilities::createSampler(_device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
	
	// TODO: object classes could create their descriptor layout.
	createObjectsDescriptors();
	createSkyboxDescriptors();
	createShadowDescriptors();
	/// Pipeline.
	createPipelines(finalRenderPass);
	createShadowPipeline(_shadowPass.renderPass);
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
	VkDeviceSize bufferSize2 = sizeof(LightInfos);
	VulkanUtilities::createBuffer(physicalDevice, _device, bufferSize2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _lightUniformBuffer, _lightUniformBufferMemory);
	
	// Create descriptor pools.
	// 2 pools: one for uniform, one for image+sampler.
	const uint32_t objectsCount = static_cast<uint32_t>(_objects.size()*2)+1;
	std::array<VkDescriptorPoolSize, 2> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = objectsCount*4;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = objectsCount*4;
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
		object.generateDescriptorSets(_device, _objectDescriptorSetLayout, _shadowDescriptorSetLayout, _descriptorPools, _uniformBuffers, _lightUniformBuffer, _shadowPass.depthView);
	}
	_skybox.generateDescriptorSets(_device, _skyboxDescriptorSetLayout, _descriptorPools, _uniformBuffers);
	
	
	// Create a command buffer and the semaphore.
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;
	if(vkAllocateCommandBuffers(_device, &allocInfo, &_shadowPass.commandBuffer) != VK_SUCCESS) {
		std::cerr << "Unable to create command buffers." << std::endl;
	}
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_shadowPass.semaphore);
	
	// Shadow pass.
	// Fill the command buffer.
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	if(vkBeginCommandBuffer(_shadowPass.commandBuffer, &beginInfo) != VK_SUCCESS) {
		std::cerr << "Unable to begin recording command buffer." << std::endl;
	}
	VkDeviceSize offsets[1] = { 0 };
	// Render pass commands.
	VkRenderPassBeginInfo renderPassInfos = {};
	renderPassInfos.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfos.renderPass = _shadowPass.renderPass;
	renderPassInfos.framebuffer = _shadowPass.frameBuffer;
	renderPassInfos.renderArea.offset = { 0, 0 };
	renderPassInfos.renderArea.extent = { static_cast<uint32_t>(_shadowPass.size[0]), static_cast<uint32_t>(_shadowPass.size[1])};
	std::array<VkClearValue, 1> clearValues = {};
	clearValues[0].depthStencil = {1.0f, 0};
	renderPassInfos.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfos.pClearValues = clearValues.data();
	vkCmdBeginRenderPass(_shadowPass.commandBuffer, &renderPassInfos, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(_shadowPass.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipeline);

	for(auto & object : _objects){
		vkCmdBindVertexBuffers(_shadowPass.commandBuffer, 0, 1, &object._vertexBuffer, offsets);
		vkCmdBindIndexBuffer(_shadowPass.commandBuffer, object._indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		// Uniform descriptor sets.
		vkCmdBindDescriptorSets(_shadowPass.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipelineLayout, 0, 1, &object.shadowDescriptorSet(), 0, nullptr);
		vkCmdPushConstants(_shadowPass.commandBuffer, _shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, (16+1)*4, &object.infos);
		vkCmdDrawIndexed(_shadowPass.commandBuffer, object._count, 1, 0, 0, 0);
	}
	vkCmdEndRenderPass(_shadowPass.commandBuffer);
	vkEndCommandBuffer(_shadowPass.commandBuffer);
}

void Renderer::createObjectsDescriptors(){
	// Descriptor layout for standard objects.
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
	// Image+sampler binding.
	VkDescriptorSetLayoutBinding samplerLayoutNormalBinding = {};
	samplerLayoutNormalBinding.binding = 2;
	samplerLayoutNormalBinding.descriptorCount = 1;
	samplerLayoutNormalBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutNormalBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutNormalBinding.pImmutableSamplers = &_textureSampler;
	
	VkDescriptorSetLayoutBinding uboLayoutLightBinding = {};
	uboLayoutLightBinding.binding = 3;
	uboLayoutLightBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutLightBinding.descriptorCount = 1;
	uboLayoutLightBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	
	VkDescriptorSetLayoutBinding samplerLayoutShadowmapBinding = {};
	samplerLayoutShadowmapBinding.binding = 4;
	samplerLayoutShadowmapBinding.descriptorCount = 1;
	samplerLayoutShadowmapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutShadowmapBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutShadowmapBinding.pImmutableSamplers = &_shadowPass.depthSampler;
	// Create the layout (== defining a struct)
	std::array<VkDescriptorSetLayoutBinding, 5> bindings = {uboLayoutBinding, uboLayoutLightBinding, samplerLayoutBinding, samplerLayoutNormalBinding, samplerLayoutShadowmapBinding};
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

void Renderer::createShadowDescriptors(){
	// Uniform binding.
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;// binding in 0.
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	
	// Create the layout (== defining a struct)
	std::array<VkDescriptorSetLayoutBinding, 1> bindings = {uboLayoutBinding};
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();
	if (vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &_shadowDescriptorSetLayout) != VK_SUCCESS) {
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
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
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
	rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	// Multisampling (none).
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	// Depth/stencil.
	// For the skybox we use a trick where the vertices are sent to the maximal depth.
	// We don't write the skybox depth to the depth buffer, and we only accept fragments for which the depth is equal to the cleared depth (1.0).
	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_EQUAL;
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


void Renderer::createShadowPipeline(const VkRenderPass & shadowRenderPass){
	// This is independent from the RTs.
	/// Shaders.
	VkShaderModule vertShaderModule = VulkanUtilities::createShaderModule(_device, "resources/shaders/compiled/shadow.vert.spv");
	//VkShaderModule fragShaderModule = VulkanUtilities::createShaderModule(_device, "resources/shaders/compiled/shadow.frag.spv");
	// Vertex shader module.
	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";
	// Fragment shader module.
	//VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	//fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	//fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	//fragShaderStageInfo.module = fragShaderModule;
	//fragShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo };
	
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
	viewport.x = 0.0f; viewport.y = 0.0f;
	viewport.width = _shadowPass.size[0];
	viewport.height = _shadowPass.size[1];
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = {(uint32_t)_shadowPass.size[0], (uint32_t)_shadowPass.size[1]};
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
	//VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	//colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	//colorBlendAttachment.blendEnable = VK_FALSE;
	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 0;
	//colorBlending.pAttachments = &colorBlendAttachment;
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
	pipelineLayoutInfo.pSetLayouts = &_shadowDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	if(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_shadowPipelineLayout) != VK_SUCCESS) {
		std::cerr << "Unable to create pipeline layout." << std::endl;
		return;
	}
	// And the pipeline.
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 1;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr;
	pipelineInfo.layout = _shadowPipelineLayout;
	pipelineInfo.renderPass = shadowRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional
	if(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_shadowPipeline) != VK_SUCCESS) {
		std::cerr << "Unable to create graphics pipeline." << std::endl;
		
	}
	
	//vkDestroyShaderModule(_device, fragShaderModule, nullptr);
	vkDestroyShaderModule(_device, vertShaderModule, nullptr);
}

void Renderer::encode(VkCommandBuffer & commandBuffer, VkQueue & graphicsQueue, VkSemaphore & imageAvailableSemaphore, VkRenderPassBeginInfo & finalRenderPassInfos, VkSubmitInfo & submitInfo, const uint32_t index){
	
	// Update uniforms.
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
	CameraInfos ubo = {};
	ubo.view = _camera.view();
	ubo.proj = _camera.projection();
	ubo.proj[1][1] *= -1; // Flip compared to OpenGL.
	LightInfos light = {};
	light.mvp = _lightViewproj;
	light.viewSpaceDir = glm::vec3(glm::normalize(ubo.view * _worldLightDir));
	// Send data.
	void* data;
	vkMapMemory(_device, _uniformBuffersMemory[index], 0, sizeof(ubo)+sizeof(light), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	memcpy(static_cast<char*>(data) + VulkanUtilities::nextOffset(sizeof(CameraInfos)), &light, sizeof(light));
	vkUnmapMemory(_device, _uniformBuffersMemory[index]);
	vkMapMemory(_device, _lightUniformBufferMemory, 0, sizeof(light), 0, &data);
	memcpy(data, &light, sizeof(light));
	vkUnmapMemory(_device, _lightUniformBufferMemory);
	
	VkSubmitInfo submitInfoShadow = {};
	submitInfoShadow.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfoShadow.waitSemaphoreCount = 1;
	submitInfoShadow.pWaitSemaphores = &imageAvailableSemaphore;
	submitInfoShadow.pWaitDstStageMask = waitStages;
	submitInfoShadow.commandBufferCount = 1;
	submitInfoShadow.pCommandBuffers = &_shadowPass.commandBuffer;
	submitInfoShadow.pSignalSemaphores = &_shadowPass.semaphore;
	submitInfoShadow.signalSemaphoreCount = 1;
	submitInfoShadow.commandBufferCount = 1;
	submitInfoShadow.pCommandBuffers = &_shadowPass.commandBuffer;
	vkQueueSubmit(graphicsQueue, 1, &submitInfoShadow, VK_NULL_HANDLE);
	
	
	VkDeviceSize offsets[1] = { 0 };
	// Final pass.
	vkCmdBeginRenderPass(commandBuffer, &finalRenderPassInfos, VK_SUBPASS_CONTENTS_INLINE);
	// Bind and draw.
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _objectPipeline);
	for(auto & object : _objects){
		VkBuffer vertexBuffers[] = {object._vertexBuffer};
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, object._indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		// Uniform descriptor sets.
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _objectPipelineLayout, 0, 1, &object.descriptorSet(index), 0, nullptr);
		vkCmdPushConstants(commandBuffer, _objectPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, (16+1)*4, &object.infos);
		vkCmdDrawIndexed(commandBuffer, object._count, 1, 0, 0, 0);
	}
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipeline);
	VkBuffer vertexBuffers[] = {_skybox._vertexBuffer};
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, _skybox._indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	// Uniform descriptor sets.
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipelineLayout, 0, 1, &_skybox.descriptorSet(index), 0, nullptr);
	vkCmdPushConstants(commandBuffer, _skyboxPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, (16+1)*4, &_skybox.infos.model);
	vkCmdDrawIndexed(commandBuffer, _skybox._count, 1, 0, 0, 0);
	
	vkCmdEndRenderPass(commandBuffer);
	
	
	if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		std::cerr << "Unable to end recording command buffer." << std::endl;
	}
	// Submit command buffer.
	submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &_shadowPass.semaphore;
	
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
	//vkDestroyPipeline(_device, _shadowPipeline, nullptr);
	//vkDestroyPipelineLayout(_device, _shadowPipelineLayout, nullptr);
	createPipelines(finalRenderPass);
}

void Renderer::clean(){
	vkDestroyPipeline(_device, _objectPipeline, nullptr);
	vkDestroyPipelineLayout(_device, _objectPipelineLayout, nullptr);
	vkDestroyPipeline(_device, _skyboxPipeline, nullptr);
	vkDestroyPipelineLayout(_device, _skyboxPipelineLayout, nullptr);
	vkDestroyPipeline(_device, _shadowPipeline, nullptr);
	vkDestroyPipelineLayout(_device, _shadowPipelineLayout, nullptr);
	vkDestroySampler(_device, _textureSampler, nullptr);
	for(size_t i = 0; i < _descriptorPools.size(); ++i){
		vkDestroyDescriptorPool(_device, _descriptorPools[i], nullptr);
	}
	vkDestroyDescriptorSetLayout(_device, _objectDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(_device, _skyboxDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(_device, _shadowDescriptorSetLayout, nullptr);
	for (size_t i = 0; i < _uniformBuffers.size(); i++) {
		vkDestroyBuffer(_device, _uniformBuffers[i], nullptr);
		vkFreeMemory(_device, _uniformBuffersMemory[i], nullptr);
	}
	for(auto & object : _objects){
		object.clean(_device);
	}
	_skybox.clean(_device);
	
	vkDestroySampler(_device, _shadowPass.depthSampler, nullptr);
	vkDestroyImageView(_device, _shadowPass.depthView, nullptr);
	vkDestroyImage(_device, _shadowPass.depthImage, nullptr);
	vkFreeMemory(_device, _shadowPass.depthMemory, nullptr);
	vkDestroyFramebuffer(_device, _shadowPass.frameBuffer, nullptr);
	vkDestroyRenderPass(_device, _shadowPass.renderPass, nullptr);
	vkDestroySemaphore(_device, _shadowPass.semaphore, nullptr);
}

