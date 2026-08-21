# Architecture Overview

## High-Level Design

VkEngine is organized into interconnected subsystems coordinated by the Engine class. All major systems (rendering, physics, audio) operate on a shared set of game objects within scenes.

```
┌─────────────────────────────────────────────────────────┐
│                      Engine                             │
│  - Lifecycle management (init/update/render/cleanup)   │
│  - Subsystem coordination                               │
│  - Scene loading/unloading                              │
└─────────────────────────────────────────────────────────┘
         ↓              ↓              ↓              ↓
    ┌────────────┐ ┌─────────────┐ ┌────────┐ ┌──────────┐
    │  Vulkan    │ │   PhysX     │ │  FMOD  │ │   GLFW   │
    │ Rendering  │ │   Physics   │ │ Audio  │ │  Input   │
    └────────────┘ └─────────────┘ └────────┘ └──────────┘
         ↓              ↓              ↓              ↓
    ┌─────────────────────────────────────────────────────┐
    │                  Active Scene                       │
    │  - Game Objects                                     │
    │  - Resources (Meshes, Textures, Sounds)            │
    │  - Custom game logic                                │
    └─────────────────────────────────────────────────────┘
```

## Core Classes

### Engine
The central coordinator singleton. Key responsibilities:
- **Initialization**: Vulkan context setup, PhysX world creation, FMOD system initialization, window creation
- **Scene Management**: Loading/unloading scenes, tracking active scene
- **Resource Management**: Creating and tracking meshes, textures, sounds
- **Object Creation**: Factory for game objects and scenes with custom allocator
- **Physics**: Managing PhysX scene, raycasts, triggers
- **Audio**: Managing FMOD system and spatial audio
- **Input**: Polling GLFW, DualSense, gamepad state
- **Rendering**: Recording Vulkan command buffers, managing frame synchronization
- **Cleanup**: Deferred destruction of resources and objects via queues

**Access Pattern**:
```cpp
Engine* engine = Engine::Create();
engine->init(width, height, "title");
// ... use engine
engine->cleanup();
Engine::Destroy(engine);
```

### Scene
Container for objects and resources in a logical grouping (typically a level). Developers inherit from Scene to customize behavior:

**Initialization**:
- `EarlyInitScene(Engine*)` - Called first, before resource loading; good for requesting resources
- `InitScene(Engine*)` - Called after resources are available; create game objects here

**Runtime**:
- `UpdateScene(Engine*)` - Called every frame for scene-specific logic
- `DestroyScene(Engine*)` - Called during cleanup

**Resource management**:
- Scenes maintain collections of meshes, textures, and game objects
- Resources are automatically cleaned up on scene unload
- Override `CreateGameObject()` to customize object instantiation

### GameObject
The fundamental entity in VkEngine. All renderable/physical things are game objects:

**Core Properties**:
- `Transform` - Position, rotation (quaternion), scale
- `Mesh*` - 3D geometry
- `Texture*` - Surface appearance
- `name`, `tag` - Identification
- Physics integration - optional RigidActor and material
- Audio - can play sounds with spatial positioning

**Lifecycle**:
```cpp
virtual void Start(Engine*);   // Called after creation
virtual void Update(Engine*);  // Called every frame
virtual void Destroy(Engine*); // Called on cleanup
```

**Inheritance**: Developers create custom GameObject subclasses for specific types (player, enemies, pickups, etc.)

### Resources (Mesh, Texture, Sound)
All inherit from `IResource`:
- **Mesh**: 3D geometry loaded from files via Assimp, converted to Vulkan buffers and PhysX shapes
- **Texture**: Image data loaded via stb_image, stored as Vulkan images with samplers
- **Sound**: Audio clips managed by FMOD, supporting spatial positioning and effects

Resources are reference-counted per scene and cleaned up on scene unload.

### CharacterController
Specialized GameObject for player characters:
- PhysX kinematic controller with gravity and collision
- Movement with directional input
- Jump mechanics with gravity acceleration
- Vertical velocity tracking
- Integration with scene raycasting for slope handling

## Data Flow: Main Loop

```
Engine::update()
  ↓
Scene::UpdateScene()
  ↓
forEach(GameObject)
  - PhysX simulation step
  - GameObject::Update() callback
  - Collision detection
  - Audio position update
  ↓
Engine::render()
  - Record Vulkan commands per GameObject
  - Submit to GPU
  - Present frame

Frame N+1
```

## Memory Management

The engine uses a custom allocator pattern:
- `Engine::requestMemory(size)` / `Engine::freeMemory(ptr)` for allocation
- Objects store an `ObjectHeader` containing a destroy function pointer
- Proper alignment handling for user-defined types
- Deferred destruction via queues for safe cleanup during game loop

See [Ownership and Lifetimes](ownership_and_lifetimes.md) for detailed lifetime semantics.
