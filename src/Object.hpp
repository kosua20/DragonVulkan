//
//  Object.hpp
//  DragonVulkan
//
//  Created by Simon Rodriguez on 14/07/2018.
//  Copyright © 2018 Simon Rodriguez. All rights reserved.
//

#ifndef Object_hpp
#define Object_hpp

#include "common.hpp"
#include "resources/MeshUtilities.hpp"

class Object {
public:
	
	Object(const std::string & name, const float shininess);
	
	~Object();
	
	void upload(const VkPhysicalDevice & physicalDevice, const VkDevice & device, const VkCommandPool & commandPool, const VkQueue & graphicsQueue);

	void clean(VkDevice & device);
	
	void generateDescriptorSets(const VkDevice & device, const VkDescriptorSetLayout & layout, const std::vector<VkDescriptorPool> & pools, const std::vector<VkBuffer> & constants);
	
	const VkDescriptorSet & descriptorSet(const int i){ return _descriptorSets[i]; }
	
	
	VkBuffer _vertexBuffer;
	VkBuffer _indexBuffer;
	uint32_t _count;
	VkImageView _textureColorView;
	VkImageView _textureNormalView;
	ObjectInfos infos;
	
private:
	std::string _name;
	
	
	VkImage _textureColorImage;
	VkImage _textureNormalImage;
	
	VkDeviceMemory _vertexBufferMemory;
	VkDeviceMemory _indexBufferMemory;
	VkDeviceMemory _textureColorMemory;
	VkDeviceMemory _textureNormalMemory;
	std::vector<VkDescriptorSet> _descriptorSets;
};

#endif /* Object_hpp */
