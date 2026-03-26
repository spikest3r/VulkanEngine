#include "texture.h"
#include "engine.h"

Texture* Engine::createTexture(std::string name, const char* path){
	if (resources.contains(name))
    {
        throw std::runtime_error("Resource name already used: " + name);
    }

	Texture* texture = new Texture();

	texture->name = name;

	createTextureImage(path, texture->imageHandle, texture->deviceMemory);
	createTextureImageView(texture->imageView, texture->imageHandle);

	textures.push_back(texture);

	resources[name] = texture;

	return texture;
}

void Texture::destroy(void* engine) {
	auto device = *static_cast<Engine*>(engine)->getVkDevicePtr();
	vkDestroyImageView(device, imageView, nullptr);
	vkDestroyImage(device, imageHandle, nullptr);
	vkFreeMemory(device, deviceMemory, nullptr);
}

ResourceType Texture::getType() {
	return TEXTURE;
}