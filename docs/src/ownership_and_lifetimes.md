# Ownership and Lifetimes

## Memory Management Model

VkEngine uses a hierarchical ownership model with deferred destruction:

1. **Engine Ownership**: Engine singleton owns scenes, resources, and allocator
2. **Scene Ownership**: Active scene owns its game objects, physics actors, and per-scene resources
3. **Object Ownership**: Each GameObject owns its Vulkan/PhysX resources
4. **Deferred Deletion**: Objects are queued for destruction and cleaned up on the next safe point

## Game Object Lifetime

### Creation

Game objects are created within scenes using a template factory:

```cpp
// In Scene::InitScene(Engine* engine)
Mesh* mesh = engine->getMesh("player_mesh");
Texture* texture = engine->getTexture("player_texture");
PhysicsMaterial* material = engine->createPhysicsMaterial(0.5f, 0.3f, 0.2f);

auto player = engine->createGameObject<PlayerCharacter>(
    transform,           // Initial position, rotation, scale
    mesh,                // Render geometry
    texture,             // Surface appearance
    material,            // Physics properties
    true                 // isDynamic - false for static colliders
);
// Engine calls player->Start() automatically
```

### Lifecycle Phases

1. **Construction**: Object memory is allocated with custom allocator, constructor runs
2. **Engine Integration**: Engine stores object metadata (ID, physics actor, audio group)
3. **Start**: `GameObject::Start(Engine*)` called - user initialization
4. **Active**: Object participates in physics, rendering, and updates each frame
5. **Update**: `GameObject::Update(Engine*)` called every frame
6. **Destruction**: Deferred via `engine->requestDestroyGameObject(object)` or scene unload

### Destruction Pattern

Objects are never destroyed immediately. Instead:

```cpp
void MyScene::UpdateScene(Engine* engine) {
    if (shouldRemoveObject) {
        engine->requestDestroyGameObject(object);
        // object still valid here
    }
}
// Later, engine checks queues and calls:
// - object->Destroy(engine)
// - PhysX actor cleanup
// - Audio channel cleanup
// - Memory deallocation
```

This prevents iterator invalidation and double-deletion bugs during game loop execution.

## Resource Lifetime

### Creation and Caching

Resources are created and cached globally by the engine:

```cpp
// First call: loads from disk, caches result
Mesh* mesh = engine->createMesh("player_mesh", "assets/player.obj");

// Subsequent calls: returns cached instance
Mesh* sameMesh = engine->getMesh("player_mesh");
```

### Per-Scene Resources

The active scene maintains collections of available resources:
- `sceneMeshes` - Meshes used by objects in this scene
- `sceneTextures` - Textures used by objects in this scene
- `sceneGameObjects` - All instantiated objects

### Cleanup on Scene Transition

When a new scene is loaded:

```cpp
engine->loadScene(nextScene);  // Implicit unload of current scene
```

This triggers:
1. `Scene::DestroyScene()` for active scene
2. Destruction of all game objects in active scene
3. Cleanup of scene-specific resources (through resource destruction queue)
4. Initialization of new scene: `EarlyInitScene()` then `InitScene()`

## Vulkan Resource Management

### Buffers and Images

Each GPU resource (vertex buffer, index buffer, image) is owned by its containing object:

- **GameObject Rendering**: Owns mesh buffers and texture images
- **Descriptor Sets**: Allocated from global descriptor pool, per-frame recycling
- **Frame Buffering**: Uses `MAX_FRAMES_IN_FLIGHT = 2` with dual-buffered command buffers and synchronization primitives
- **Synchronization**: Fences and semaphores prevent CPU-GPU synchronization hazards

### Swapchain Management

- Created during `Engine::init()`
- Recreated on window resize via framebuffer callback
- Images owned by GLFW/Vulkan driver, not VkEngine

## PhysX Resource Management

### Physics Actors

Each GameObject with physics owns a `PxRigidActor` (PxRigidStatic or PxRigidDynamic):

```cpp
// Created during engine->createGameObject<>() with mesh and material
// Automatically removed when object is destroyed
// Shape and material owned by PhysX internally
```

### Character Controllers

Created separately from GameObjects:

```cpp
ICharacterController* controller = engine->createCharacterController(
    height, radius, position,
    material,
    interactWithActors  // Whether to interact with dynamic objects
);

// Request destruction separately
engine->requestDestroyCharacterController(controller);
```

### Triggers

Physics-only entities without rendering:

```cpp
Trigger* trigger = engine->createBoxTrigger(position, size);
trigger->onTriggerEnter = [](GameObject* other) { /* ... */ };
trigger->onTriggerExit = [](GameObject* other) { /* ... */ };

// Request destruction when done
engine->requestDestroyTrigger(trigger);
```

## Audio Resource Management

### Sounds

Created by engine and cached globally:

```cpp
Sound* sfx = engine->createSound("footstep", "assets/step.ogg", false, true);
// false = not looping
// true = 3D spatial audio
```

### Channel Groups

Each GameObject has a private `FMOD::ChannelGroup*` for sound isolation:
- Owned by engine's FMOD system
- Destroyed when GameObject is destroyed
- Used for per-object volume and pause control

### Spatial Audio

Position and velocity updated automatically from GameObject transform each frame.

## Memory Allocation Strategy

### Custom Allocator

The engine uses a tracking allocator (if enabled):

```cpp
void* Engine::requestMemory(size_t size);
void Engine::freeMemory(void* ptr);
```

With object header pattern for type-safe destruction:

```cpp
struct ObjectHeader {
    void (*destroy)(void*);   // Function pointer to typed destructor
    void* allocationBase;     // Pointer to raw allocation
};
```

Developers using engine APIs don't need to manage this directly - it's handled internally.

### Container Allocators

STL containers in scenes use `EngineAllocator<T>`:
- Simple malloc/free wrapper
- Allows optional memory tracking for debugging
- Applied to scene resource collections

## Lifetime Rules Summary

| Resource | Created By | Owned By | Destroyed By | When |
|----------|-----------|----------|--------------|------|
| GameObject | `engine->createGameObject<>()` | Engine | `requestDestroyGameObject()` or scene unload | Next frame |
| Mesh | `engine->createMesh()` | Engine (global cache) | Manual via `requestDestroy()` or engine cleanup | On request or exit |
| Texture | `engine->createTexture()` | Engine (global cache) | Manual via `requestDestroy()` or engine cleanup | On request or exit |
| Sound | `engine->createSound()` | Engine (global cache) | Manual via `requestDestroy()` or engine cleanup | On request or exit |
| Scene | `engine->createScene<>()` | Engine | `requestDestroyScene()` or manual | On request or exit |
| Trigger | `engine->createBoxTrigger()` | Engine | `requestDestroyTrigger()` | On request or exit |
| CharacterController | `engine->createCharacterController()` | Engine | `requestDestroyCharacterController()` | On request or exit |
| PhysicsMaterial | `engine->createPhysicsMaterial()` | Engine | Manual via `requestDestroy()` or engine cleanup | On request or exit |
