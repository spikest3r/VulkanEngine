# Resource Management

## Resource System Overview

VkEngine has a unified resource system for all asset types. Resources are created, cached globally, and referenced by GameObjects and Scenes. Cleanup happens automatically on scene unload or via explicit request.

## Resource Types

### Mesh
3D geometry data with vertices and indices.

**Properties**:
- Vertices (position, color, texture coordinates)
- Index buffer for face definitions
- Vulkan GPU buffers (VkBuffer for vertices/indices)
- PhysX collision meshes (convex for dynamic objects, triangle mesh for static)
- Supports multiple meshes per file via Assimp

**Creation**:
```cpp
// Load from file (OBJ, FBX, GLTF, etc.)
Mesh* mesh = engine->createMesh("player", "assets/player.obj");

// Create from vertex data
std::vector<Vertex> vertices = { /* ... */ };
std::vector<uint32_t> indices = { /* ... */ };
Mesh* custom = engine->createMesh("custom", vertices, indices);
```

**Retrieval**:
```cpp
Mesh* mesh = engine->getMesh("player");  // Returns cached instance
```

**Supported Formats**: OBJ, FBX, GLTF, DAE, BLEND (via Assimp support list)

**Vertex Structure**:
```cpp
struct Vertex {
    glm::vec4 pos;      // Position + padding
    glm::vec3 color;    // Vertex color
    glm::vec2 texCoord; // Texture coordinates
};
```

### Texture
2D image data for rendering surfaces.

**Properties**:
- Loaded from disk via stb_image
- Stored as Vulkan VkImage with VkImageView
- Linear or optimal tiling based on device
- Descriptor set for shader binding
- Sampler for filtering and wrapping

**Creation**:
```cpp
Texture* tex = engine->createTexture("rock", "assets/rock.png");
```

**Retrieval**:
```cpp
Texture* tex = engine->getTexture("rock");
```

**Supported Formats**: PNG, JPG, TGA, BMP (via stb_image)

### Sound
Audio clip for playback via FMOD.

**Properties**:
- Loaded into FMOD system
- Can be looping or one-shot
- 2D or 3D (spatial)
- Mono or stereo
- Supports compression formats

**Creation**:
```cpp
Sound* sfx = engine->createSound(
    "footstep",           // Name
    "assets/step.ogg",    // Path
    false,                // looping = false for one-shot
    true                  // three_dim = true for spatial audio
);
```

**Retrieval**:
```cpp
Sound* sfx = engine->getSound("footstep");
```

**Supported Formats**: OGG, WAV, MP3, FLAC, etc. (depends on FMOD installation)

## Resource Interface

All resources inherit from `IResource`:

```cpp
class IResource {
public:
    virtual ~IResource();
    virtual void destroy(void*) = 0;
    virtual ResourceType getType() = 0;
    std::string getName();
};
```

Each resource type implements:
- `destroy()` - Vulkan/FMOD cleanup
- `getType()` - Returns TEXTURE, MESH, or SOUND
- Automatic management by engine

## Resource Lifecycle

### Creation and Caching

All resources are created at engine level and globally cached:

```cpp
// First call: loads from disk
Mesh* mesh1 = engine->createMesh("rock", "assets/rock.obj");

// Subsequent calls: returns cache (no reload)
Mesh* mesh2 = engine->getMesh("rock");
assert(mesh1 == mesh2);  // Same pointer

// Multiple references in same scene are okay
scene->obj1->mesh = mesh1;
scene->obj2->mesh = mesh1;  // Shared mesh
```

### Loading Phases

Resources are typically loaded during scene initialization:

```cpp
class Level1Scene : public Scene {
    void EarlyInitScene(Engine* engine) override {
        // Called first - request assets
        // Good for spawning background load tasks
    }
    
    void InitScene(Engine* engine) override {
        // Called after assets available - use them
        mesh = engine->createMesh("level", "assets/level1.obj");
        texture = engine->createTexture("floor", "assets/floor.png");
    }
};
```

### Cleanup

Resources are cleaned up when:
1. Scene unloads (scene-specific resources are cleaned)
2. Explicit destruction requested

```cpp
// Request deferred destruction
engine->requestDestroy(myTexture);

// Actually destroyed on next frame's cleanup phase
```

## Scene Resource Organization

Each scene maintains collections of resources it uses:

```cpp
class Scene {
private:
    std::vector<SceneResource> sceneMeshes;      // All meshes in scene
    std::vector<SceneResource> sceneTextures;    // All textures in scene
    std::vector<SceneGameObject> sceneGameObjects; // All objects in scene
};
```

These are managed automatically by the engine when objects reference them.

## Memory Strategy

### Vulkan GPU Memory

- **Buffers**: Mesh vertex/index buffers allocated and bound at creation
- **Images**: Textures uploaded to GPU memory with optimal tiling
- **Descriptor Sets**: Allocated from per-frame descriptor pool, recycled each frame
- **Synchronization**: Staging buffers used for CPU→GPU transfer

Current implementation uses direct Vulkan allocation (VMA integration is noted as TODO in code).

### FMOD Audio Memory

- Sounds kept resident in system memory
- Streaming option available at creation time for large files
- Memory managed by FMOD internally

### Asset Organization Best Practices

```
assets/
├── models/
│   ├── player.obj
│   ├── enemy.obj
│   └── level1.obj
├── textures/
│   ├── player.png
│   ├── enemy.png
│   └── floor.png
├── sounds/
│   ├── step_concrete.ogg
│   ├── step_metal.ogg
│   └── ambient_wind.ogg
├── scenes/
│   ├── level1.scene
│   └── level2.scene
└── fonts/
    └── ui_font.ttf
```

## Scene Callbacks

Scenes can customize resource loading behavior:

```cpp
class CustomScene : public Scene {
    void ResourceLoaded(std::string name, const char* path, ResourceType type) override {
        // Called when a resource finishes loading
        if (type == MESH && name == "player") {
            onPlayerModelLoaded();
        }
    }
    
    GameObject* CreateGameObject(Engine* engine, 
                                 const char* objectType, const char* tag,
                                 const char* name, Transform transform,
                                 Mesh* mesh, Texture* texture,
                                 bool dynamic) override {
        // Called when creating objects from scene file
        // Can instantiate custom GameObject subclasses based on objectType
        
        if (strcmp(objectType, "enemy") == 0) {
            return engine->createGameObject<Enemy>(transform, mesh, texture, ...);
        }
        return engine->createGameObject<GameObject>(transform, mesh, texture, ...);
    }
};
```

## Memory Management Rules

| Type | Cached Globally | Per-Scene Copy | Lifetime |
|------|-----------------|-----------------|----------|
| Mesh | Yes | No | Until explicit destroy() or engine cleanup |
| Texture | Yes | No | Until explicit destroy() or engine cleanup |
| Sound | Yes | No | Until explicit destroy() or engine cleanup |
| GameObject | No | Yes | Scene lifetime |
| PhysicsMaterial | No | Yes | Reference held by objects |

**Key Rule**: Don't destroy resources while scenes are using them. Request destruction, and let engine handle cleanup order.
