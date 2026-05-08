#include "engine.h"

void Engine::checkResourceDestroy() {
	if (destroyQueue.empty()) return;

	vkDeviceWaitIdle(device);

	while (!destroyQueue.empty()) {
		try {
			auto& res = destroyQueue.front();

			std::string resName = res->getName();
			resources.erase(resName);

			switch (res->getType()) {
			case TEXTURE:
			{
				auto* it = static_cast<Texture*>(res);
				it->destroy(this);
				delete it;
				std::swap(textures.back(), it);
				textures.pop_back();
				textures.shrink_to_fit();
				break;
			}
			case SOUND:
			{
				auto* it = static_cast<Sound*>(res);
				it->destroy(this);
				delete it;
				std::swap(sounds.back(), it);
				sounds.pop_back();
				sounds.shrink_to_fit();
				break;
			}
			case MESH: {
				auto* it = static_cast<Mesh*>(res);
				it->destroy(this);
				delete it;
				std::swap(meshes.back(), it);
				meshes.pop_back();
				meshes.shrink_to_fit();
				break;
			}
			}
		}
		catch (std::exception ex) {
			std::cout << "Resource destroy error!" << std::endl;
			std::cout << ex.what() << std::endl;
		}

		destroyQueue.pop();
	}
}

void Engine::requestDestroy(IResource* resource) {
	destroyQueue.push(resource);
}

Texture* Engine::getTexture(std::string name) {
    auto it = resources.find(name);
    if (it == resources.end())
        return nullptr;

    return dynamic_cast<Texture*>(it->second);
}

Mesh* Engine::getMesh(std::string name) {
    auto it = resources.find(name);
    if (it == resources.end())
        return nullptr;

	Mesh* mesh = dynamic_cast<Mesh*>(it->second);
	if (mesh && mesh->engineMember) return nullptr; // do not return engine members
    return mesh;
}

Sound* Engine::getSound(std::string name) {
    auto it = resources.find(name);
    if (it == resources.end())
        return nullptr;

    return dynamic_cast<Sound*>(it->second);
}

void Engine::forceDestroy() {
	checkDestroy();
}