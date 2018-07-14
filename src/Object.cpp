//
//  Object.cpp
//  DragonVulkan
//
//  Created by Simon Rodriguez on 14/07/2018.
//  Copyright © 2018 Simon Rodriguez. All rights reserved.
//

#include "Object.hpp"
#include "VulkanUtilities.hpp"

Object::~Object() {  }

Object::Object(const std::string &name) {
	const std::string meshPath = "resources/meshes/" + name + ".obj";
	MeshUtilities::loadObj(meshPath, _mesh, MeshUtilities::Indexed);
//	MeshUtilities::centerAndUnitMesh(_mesh);// TODO temp for debug.
//	MeshUtilities::computeTangentsAndBinormals(_mesh);
}

void Object::upload(VkPhysicalDevice & physicalDevice, VkDevice & device, VkCommandPool & commandPool, VkQueue & graphicsQueue) {
	
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
	//_mesh.vertices.clear();
	//_mesh.indices.clear();
	//_mesh = {};
}

void Object::clean(VkDevice & device){
	vkDestroyBuffer(device, _vertexBuffer, nullptr);
	vkFreeMemory(device, _vertexBufferMemory, nullptr);
	vkDestroyBuffer(device, _indexBuffer, nullptr);
	vkFreeMemory(device, _indexBufferMemory, nullptr);
}


