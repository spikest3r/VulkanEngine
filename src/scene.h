#pragma once

#include "engine_types.h"
#include "3d_loader.h"
#include "texture.h"

template <typename T>
struct EngineAllocator
{
    using value_type = T;

    EngineAllocator() noexcept = default;

    template <class U>
    EngineAllocator(const EngineAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        auto size = n * sizeof(T);
        auto ptr = static_cast<T*>(std::malloc(size));
        //printf("[STALLOC] ptr=%p size=%zu type=%s\n", ptr, size, typeid(T).name());
        if (!ptr) throw std::bad_alloc();
        return ptr;
    }

    void deallocate(T* p, std::size_t n) noexcept {
        //printf("[STFREE] ptr=%p\n", p);
        std::free(p);
    }

    template <class U>
    struct rebind {
        using other = EngineAllocator<U>;
    };

    bool operator==(const EngineAllocator&) const noexcept { return true; }
    bool operator!=(const EngineAllocator&) const noexcept { return false; }
};

class Scene {
	friend class Engine;
private:
    std::vector<SceneResource, EngineAllocator<SceneResource>> sceneMeshes;
    std::vector<SceneResource, EngineAllocator<SceneResource>> sceneTextures;
    std::vector<SceneGameObject, EngineAllocator<SceneGameObject>> sceneGameObjects;

	// custom user logic
    
    // GameObjects and resources are not loaded
    ENGINE_API virtual void EarlyInitScene(Engine* engine);
	
    // GameObjects and resources are loaded
    ENGINE_API virtual void InitScene(Engine* engine);

    ENGINE_API virtual void UpdateScene(Engine* engine);
    ENGINE_API virtual void DestroyScene(Engine* engine);
	ENGINE_API virtual GameObject* CreateGameObject(Engine* engine, const char* objectType, const char* tag, const char* name, Transform transform, Mesh* mesh, Texture* texture, bool dynamic);
    ENGINE_API virtual void ResourceLoaded(std::string name, const char* path, ResourceType type);

    std::string sceneFileName;
};