# VkEngine

A high-performance 3D game engine for C++ featuring Vulkan graphics, PhysX physics, FMOD audio, and first-person character control. VkEngine provides a complete framework for building 3D games with a focus on simplicity and correctness.

![Vulkan](https://img.shields.io/badge/API-Vulkan%201.3-AC162C?style=for-the-badge&logo=vulkan&logoColor=white)
![PhysX](https://img.shields.io/badge/Physics-NVIDIA%20PhysX-76B900?style=for-the-badge&logo=nvidia&logoColor=white)
![FMOD](https://img.shields.io/badge/Audio-FMOD-00AEEF?style=for-the-badge&logo=fmod&logoColor=white)

## Quick Start

```cpp
#include <engine.h>

int main() {
    Engine* engine = Engine::Create();
    engine->init(800, 600, "My Game");
    
    // Create and load scene
    auto scene = engine->createScene<MyScene>();
    engine->loadScene(scene);
    
    // Main loop
    while (engine->running()) {
        engine->updateScene();
        engine->update();
        engine->render();
    }
    
    engine->cleanup();
    Engine::Destroy(engine);
    return 0;
}
```

## Core Features

### Rendering
- **Vulkan 1.3** - Modern GPU graphics with forward rendering
- **Double buffering** - 2 frames in flight for consistent 60 FPS
- **ImGui integration** - Debug UI and in-game overlays
- **Texture sampling** - Per-object texture binding with samplers

### Physics
- **NVIDIA PhysX** - Rigid body dynamics, collision detection
- **Character controller** - Built-in first-person movement with gravity
- **Raycasting** - World queries and picking
- **Trigger volumes** - Non-physical overlap detection

### Audio
- **FMOD Studio** - Spatial 3D audio with distance attenuation
- **Per-object sounds** - Audio follows game objects in 3D space
- **Channel groups** - Per-object audio control and mixing
- **Haptics** - DualSense controller feedback (Windows/PlayStation)

### Game Objects & Scenes
- **Entity-based system** - GameObject classes with lifecycle hooks
- **Scene management** - Load/unload scenes with automatic resource cleanup
- **Custom inheritance** - Create custom GameObject subclasses for game logic
- **Deferred destruction** - Safe resource cleanup during game loop

### Input
- **Keyboard & Mouse** - GLFW-based polling with full key mapping
- **Gamepad support** - Standard gamepad and DualSense controllers
- **Cursor control** - Hidden, locked, or normal cursor modes
- **Mouse rays** - 3D world picking from screen coordinates

## Project Structure

- **VulkanEngine/** - Core engine with all subsystems
  - `src/include/` - Public API headers
  - `src/` - Implementation
  
- **LabEscape/** - Example puzzle-escape game demonstrating all engine features
  - Complete game with multiple levels
  - Puzzles, character control, audio, physics
  - Ready to extend or use as template

- **engine-docs/** - Complete documentation
  - Architecture overview
  - API reference with examples
  - Real usage patterns from LabEscape
  - Best practices and design patterns

## Platform Support

- **Windows** - MSVC 2022, Vulkan capable GPU required
- **Linux** - GCC/Clang, X11/Wayland with Vulkan support
- **Cross-platform** - Same C++ API on both platforms

## Project Status

**Current**: Feature-complete for 3D games with character control and puzzle mechanics. Suitable for small to medium projects.

**Tested on**:
- Windows 10/11 with GTX 1060 or better
- Linux (Arch) with Vulkan-capable GPU

**Not included**:
- Deferred rendering / advanced lighting
- Asset streaming / LOD system
- Multiplayer networking
- Editor tools (ImGui debug UI included)
- Mobile platforms

## Documentation

**📖 Full documentation**: See `docs/` for comprehensive API reference, architecture guide, and tutorials. (alternatively, visit [engine.olehsheremeta.com](https://engine.olehsheremeta.com/))

Quick links:
- [Architecture Overview](../engine-docs/src/architecture.md)
- [Game Objects & Entities](../engine-docs/src/game_objects_and_entities.md)
- [Physics System](../engine-docs/src/physics.md)
- [Audio System](../engine-docs/src/audio.md)
- [LabEscape Example](../engine-docs/src/labescape_example.md)

## Usage Examples

### Creating GameObjects

```cpp
Mesh* mesh = engine->getMesh("player");
Texture* texture = engine->getTexture("player_skin");
PhysicsMaterial* material = engine->createPhysicsMaterial(0.5f, 0.3f, 0.2f);

auto player = engine->createGameObject<PlayerCharacter>(
    transform,      // Position, rotation, scale
    mesh,
    texture,
    material,
    true            // isDynamic - physics enabled
);
```

### Playing Sounds

```cpp
Sound* footstep = engine->createSound("step", "assets/step.ogg", false, true);
gameObject->playSound(footstep, 0.7f);  // 3D spatial audio from object
```

### Physics Queries

```cpp
// Raycast for picking
Vector3 rayOrigin, rayDirection;
engine->getMouseRay(rayOrigin, rayDirection);
RaycastHit hit = engine->raycast(rayOrigin, rayDirection, 100.0f);
if (hit.object) {
    hit.object->applyForce(Vector3(0, 1, 0) * 50.0f);
}

// Trigger volumes
Trigger* damageZone = engine->createBoxTrigger(position, size);
damageZone->onTriggerEnter = [](GameObject* other) {
    if (other->tag == "player") player->takeDamage(10);
};
```

### Character Control

```cpp
ICharacterController* player = engine->createCharacterController(
    1.8f,      // Height
    0.4f,      // Radius
    position,
    material,
    true       // Interact with dynamic objects
);

void Update(Engine* engine) {
    Vector3 moveDir = getInputDirection();
    player->Move(moveDir, 10.0f, engine->getDeltaTime());
    
    if (shouldJump()) {
        player->Jump(15.0f);
    }
}
```

## Critical Rules

1. **No raw delete** - All destruction goes through `engine->requestDestroy*()`
2. **One active scene** - Only one scene active at a time
3. **Resource lifecycle** - Resources outlive scenes; clean up explicitly if needed
4. **Thread safety** - Engine is not thread-safe; all calls from main thread

## Key Classes

| Class | Purpose |
|-------|---------|
| `Engine` | Central coordinator for all subsystems |
| `Scene` | Container for game objects and resources |
| `GameObject` | Renderable entity with physics and audio |
| `ICharacterController` | First-person character with gravity |
| `Mesh`, `Texture`, `Sound` | Resources managed by engine |
| `Trigger` | Non-physical overlap volume |
| `PhysicsMaterial` | Physics properties (friction, restitution) |

## Example: LabEscape

VkEngine includes **LabEscape**, a complete puzzle-escape game showcasing:
- Multi-level scene system with transitions
- Character movement and camera control
- Interactive puzzles (keypad, Simon Says, ball-in-cup)
- Physics-based interactions
- Spatial audio and sound effects
- DualSense haptic feedback
- ImGui-based debug tools

Study LabEscape's source code for real-world engine usage patterns.

## Building

Requires:
- C++17 compiler
- Vulkan SDK
- PhysX 4.x (or build from AUR on Linux)
- FMOD Core SDK
- CMake (recommended) or MSVC project

See `../engine-docs/` for detailed build instructions.

## Contributing

VkEngine is a learning/portfolio project. Contributions welcome for bug fixes and improvements.

## License

See LICENSE file in repository.

---

**For detailed documentation, examples, and API reference**, see [engine.olehsheremeta.com](https://engine.olehsheremeta.com/)