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
	
	Object(const std::string & name);
	
	~Object();
	
	void upload(VkPhysicalDevice & physicalDevice, VkDevice & device, VkCommandPool & commandPool, VkQueue & graphicsQueue);

	void clean(VkDevice & device);
	
	VkBuffer _vertexBuffer;
	VkBuffer _indexBuffer;
	uint32_t _count;
	
private:
	Mesh _mesh;
	
	VkDeviceMemory _vertexBufferMemory;
	
	VkDeviceMemory _indexBufferMemory;
};

#endif /* Object_hpp */
