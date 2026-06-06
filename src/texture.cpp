#include "texture.h"
#include "engine.h"

Texture* Engine::createTexture(std::string name, const char* path) {
    if (resources.contains(name))
        throw std::runtime_error("Resource name already used: " + name);

    Texture* texture = new Texture();
    texture->name = name;
    createTextureImage(path, texture->imageHandle, texture->deviceMemory);
    createTextureImageView(texture->imageView, texture->imageHandle);

    texture->createDescriptorSet(device, descriptorPool, textureSetLayout, textureSampler);

	assert(texture->descriptorSet != VK_NULL_HANDLE && "texture descriptorSet is null!");

    textures.push_back(texture);
    resources[name] = texture;
    return texture;
}

void Texture::createDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout, VkSampler sampler) {
    VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool     = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &layout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate texture descriptor set!");

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = imageView;  // FIX: was 'view', field is 'imageView'
    imageInfo.sampler     = sampler;

    VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstSet          = descriptorSet;
    write.dstBinding      = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void Texture::destroy(void* engine) {
    Engine* eng = static_cast<Engine*>(engine);
    VkDevice device = *eng->getVkDevicePtr();

    // FREE the descriptor set before destroying the image
	vkFreeDescriptorSets(device, eng->getDescriptorPool(), 1, &descriptorSet);

    vkDestroyImageView(device, imageView, nullptr);
    vkDestroyImage(device, imageHandle, nullptr);
    vkFreeMemory(device, deviceMemory, nullptr);
}

ResourceType Texture::getType() {
    return TEXTURE;
}