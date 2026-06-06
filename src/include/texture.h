#pragma once

#include <vulkan/vulkan.h>
#include "engine_types.h"

class Engine;
class GameObject;

class ENGINE_API Texture : public IResource {
	friend class Engine;
	friend class GameObject;
private:
	Texture() {};
	// TODO: Use VMA
	VkImage imageHandle;
	VkImageView imageView;
	VkDeviceMemory deviceMemory;
	void destroy(void*) override;
	ResourceType getType() override;
	VkDescriptorSet descriptorSet;  // moved here from GameObject
	void createDescriptorSet(VkDevice, VkDescriptorPool, VkDescriptorSetLayout, VkSampler);
};