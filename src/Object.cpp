//
//  Object.cpp
//  DragonVulkan
//
//  Created by Simon Rodriguez on 14/07/2018.
//  Copyright © 2018 Simon Rodriguez. All rights reserved.
//

#include "Object.hpp"
#include "VulkanUtilities.hpp"
#include "resources/Resources.hpp"

Object::~Object() {  }

Object::Object(const std::string &name, const float shininess) {
	_name = name;
	infos.model = glm::mat4(1.0f);
	infos.shininess = shininess;
}

void Object::upload(const VkPhysicalDevice & physicalDevice, const VkDevice & device, const VkCommandPool & commandPool, const VkQueue & graphicsQueue) {
	
	// Mesh.
	Mesh mesh;
	const std::string meshPath = "resources/meshes/" + _name + ".obj";
	MeshUtilities::loadObj(meshPath, mesh, MeshUtilities::Indexed);
	MeshUtilities::centerAndUnitMesh(mesh);
	MeshUtilities::computeTangentsAndBinormals(mesh);
	
	/// Vertex buffer.
	// TODO: bundle all of this away.
	VkDeviceSize bufferSize = sizeof(mesh.vertices[0]) * mesh.vertices.size();
	
	// Use a staging buffer as an intermediate.
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VulkanUtilities::createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	// Fill it.
	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, mesh.vertices.data(), (size_t) bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);
	// Create the destination buffer.
	VulkanUtilities::createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vertexBuffer, _vertexBufferMemory);
	// Copy from the staging buffer to the final.
	// TODO: use specific command pool.
	VulkanUtilities::copyBuffer(stagingBuffer, _vertexBuffer, bufferSize, device, commandPool, graphicsQueue);
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
	
	/// Index buffer.
	bufferSize = sizeof(mesh.indices[0]) * mesh.indices.size();
	// Create and fill the staging buffer.
	VulkanUtilities::createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, mesh.indices.data(), (size_t) bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);
	// Create and copy final buffer.
	VulkanUtilities::createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _indexBuffer, _indexBufferMemory);
	VulkanUtilities::copyBuffer(stagingBuffer, _indexBuffer, bufferSize, device, commandPool, graphicsQueue);
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
	
	_count  = static_cast<uint32_t>(mesh.indices.size());
	
	/// Textures.
	unsigned int texWidth, texHeight, texChannels;
	void* image;
	int rett = Resources::loadImage("resources/textures/" + _name + "_texture_color.png", texWidth, texHeight, texChannels, &image, true);
	if(rett != 0){ std::cerr << "Error loading color image." << std::endl; }
	VulkanUtilities::createTexture(image, texWidth, texHeight, physicalDevice, device, commandPool, graphicsQueue, _textureColorImage, _textureColorMemory, _textureColorView);
	free(image);
	
	rett = Resources::loadImage("resources/textures/" + _name + "_texture_normal.png", texWidth, texHeight, texChannels, &image, true);
	if(rett != 0){ std::cerr << "Error loading normal image." << std::endl; }
	VulkanUtilities::createTexture(image, texWidth, texHeight, physicalDevice, device, commandPool, graphicsQueue, _textureNormalImage, _textureNormalMemory, _textureNormalView);
	free(image);
}

void Object::generateDescriptorSets(const VkDevice & device, const VkDescriptorSetLayout & layout, const std::vector<VkDescriptorPool> & pools, const std::vector<VkBuffer> & constants){
	
	_descriptorSets.resize(pools.size());
	
	for (size_t i = 0; i < _descriptorSets.size(); i++) {
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pools[i];
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout;
		
		if (vkAllocateDescriptorSets(device, &allocInfo, &_descriptorSets[i]) != VK_SUCCESS) {
			std::cerr << "Unable to create descriptor sets." << std::endl;
		}
		
		VkDescriptorBufferInfo bufferCameraInfo = {};
		bufferCameraInfo.buffer = constants[i];
		bufferCameraInfo.offset = 0;
		bufferCameraInfo.range = sizeof(CameraInfos);
		VkDescriptorBufferInfo bufferLightInfo = {};
		bufferLightInfo.buffer = constants[i];
		bufferLightInfo.offset = VulkanUtilities::nextOffset(sizeof(CameraInfos));
		bufferLightInfo.range = sizeof(LightInfos);
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = _textureColorView;
		VkDescriptorImageInfo imageNormalInfo = {};
		imageNormalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageNormalInfo.imageView = _textureNormalView;
		
		std::array<VkWriteDescriptorSet, 4> descriptorWrites = {};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = _descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferCameraInfo;
		
		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = _descriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;
		
		descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[2].dstSet = _descriptorSets[i];
		descriptorWrites[2].dstBinding = 2;
		descriptorWrites[2].dstArrayElement = 0;
		descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[2].descriptorCount = 1;
		descriptorWrites[2].pImageInfo = &imageNormalInfo;
		
		descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[3].dstSet = _descriptorSets[i];
		descriptorWrites[3].dstBinding = 3;
		descriptorWrites[3].dstArrayElement = 0;
		descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[3].descriptorCount = 1;
		descriptorWrites[3].pBufferInfo = &bufferLightInfo;
		
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

void Object::clean(VkDevice & device){
	vkDestroyImageView(device, _textureColorView, nullptr);
	vkDestroyImage(device, _textureColorImage, nullptr);
	vkFreeMemory(device, _textureColorMemory, nullptr);
	vkDestroyImageView(device, _textureNormalView, nullptr);
	vkDestroyImage(device, _textureNormalImage, nullptr);
	vkFreeMemory(device, _textureNormalMemory, nullptr);
	
	vkDestroyBuffer(device, _vertexBuffer, nullptr);
	vkFreeMemory(device, _vertexBufferMemory, nullptr);
	vkDestroyBuffer(device, _indexBuffer, nullptr);
	vkFreeMemory(device, _indexBufferMemory, nullptr);
}


