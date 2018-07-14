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
	
	VkBuffer _vertexBuffer;
	VkBuffer _indexBuffer;
	uint32_t _count;
	VkImageView _textureImageView;
	
	struct Data {
		glm::mat4 model;
		float shininess;
	} infos;
	
private:
	std::string _name;
	
	
	VkImage _textureImage;
	
	VkDeviceMemory _vertexBufferMemory;
	VkDeviceMemory _indexBufferMemory;
	VkDeviceMemory _textureImageMemory;
};

#endif /* Object_hpp */
