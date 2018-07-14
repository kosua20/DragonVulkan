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

Object::Object(const std::string &name) {
	const std::string meshPath = "resources/meshes/" + name + ".obj";
	MeshUtilities::loadObj(meshPath, _mesh, MeshUtilities::Indexed);
//	MeshUtilities::centerAndUnitMesh(_mesh);// TODO temp for debug.
//	MeshUtilities::computeTangentsAndBinormals(_mesh);
}

void Object::upload(const VkPhysicalDevice & physicalDevice, const VkDevice & device, const VkCommandPool & commandPool, const VkQueue & graphicsQueue) {
	
	/// Vertex buffer.
	// TODO: bundle all of this away.
	VkDeviceSize bufferSize = sizeof(_mesh.vertices[0]) * _mesh.vertices.size();
	
	// Use a staging buffer as an intermediate.
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VulkanUtilities::createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	// Fill it.
	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, _mesh.vertices.data(), (size_t) bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);
	// Create the destination buffer.
	VulkanUtilities::createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vertexBuffer, _vertexBufferMemory);
	// Copy from the staging buffer to the final.
	// TODO: use specific command pool.
	VulkanUtilities::copyBuffer(stagingBuffer, _vertexBuffer, bufferSize, device, commandPool, graphicsQueue);
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
	
	/// Index buffer.
	bufferSize = sizeof(_mesh.indices[0]) * _mesh.indices.size();
	// Create and fill the staging buffer.
	VulkanUtilities::createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, _mesh.indices.data(), (size_t) bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);
	// Create and copy final buffer.
	VulkanUtilities::createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _indexBuffer, _indexBufferMemory);
	VulkanUtilities::copyBuffer(stagingBuffer, _indexBuffer, bufferSize, device, commandPool, graphicsQueue);
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
	
	_count  = static_cast<uint32_t>(_mesh.indices.size());
	
	// Clear the mesh.
	_mesh.vertices.clear();
	_mesh.indices.clear();
	_mesh = {};
	
	/// Texture.
	unsigned int texWidth, texHeight, texChannels;
	void* image;
	int rett = Resources::loadImage("resources/statue.png", texWidth, texHeight, texChannels, &image, false);
	if(rett != 0){ std::cerr << "Error loading image." << std::endl; }
	VkDeviceSize imageSize = texWidth * texHeight * 4;
	VkBuffer stagingBufferImg;
	VkDeviceMemory stagingBufferMemoryImg;
	VulkanUtilities::createBuffer(physicalDevice, device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBufferImg, stagingBufferMemoryImg);
	void* dataImg;
	vkMapMemory(device, stagingBufferMemoryImg, 0, imageSize, 0, &dataImg);
	memcpy(dataImg, image, static_cast<size_t>(imageSize));
	vkUnmapMemory(device, stagingBufferMemoryImg);
	free(image);
	// Create texture image.
	VulkanUtilities::createImage(physicalDevice, device, texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _textureImage, _textureImageMemory);
	// Prepare the image layout for the transfer (we don't care about what's in it before the copy).
	VulkanUtilities::transitionImageLayout(device, commandPool, graphicsQueue, _textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	// Copy from the buffer to the image.
	VulkanUtilities::copyBufferToImage(stagingBufferImg, _textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), device, commandPool, graphicsQueue);
	// Optimize the layout of the image for sampling.
	VulkanUtilities::transitionImageLayout(device, commandPool, graphicsQueue, _textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkDestroyBuffer(device, stagingBufferImg, nullptr);
	vkFreeMemory(device, stagingBufferMemoryImg, nullptr);
	// Create texture view.
	_textureImageView = VulkanUtilities::createImageView(device, _textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Object::clean(VkDevice & device){
	vkDestroyImageView(device, _textureImageView, nullptr);
	vkDestroyImage(device, _textureImage, nullptr);
	vkFreeMemory(device, _textureImageMemory, nullptr);
	
	vkDestroyBuffer(device, _vertexBuffer, nullptr);
	vkFreeMemory(device, _vertexBufferMemory, nullptr);
	vkDestroyBuffer(device, _indexBuffer, nullptr);
	vkFreeMemory(device, _indexBufferMemory, nullptr);
}


