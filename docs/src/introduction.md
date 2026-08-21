# Introduction

VkEngine is a C++ game engine built on Vulkan for high-performance 3D graphics. It provides a complete game development framework including rendering, physics simulation, audio playback, and scene management. LabEscape, a puzzle-escape game included with the engine, demonstrates all major features in a production example.

## Key Features

- **Vulkan-based Rendering**: Modern graphics API for cross-platform high-performance rendering with ImGui integration
- **Physics Simulation**: NVIDIA PhysX integration for realistic physics, collision detection, and character controllers
- **Spatial Audio**: FMOD integration for 3D sound with distance attenuation and positional effects
- **Scene System**: Flexible scene management with virtual initialization, update, and cleanup hooks
- **Game Objects**: C++ entity system with transforms, physics, audio, and rendering
- **Character Controller**: First-person character movement with gravity, jumping, and terrain interaction
- **Resource Management**: Unified system for meshes, textures, and sounds with lazy loading
- **Input Handling**: Keyboard, mouse, and DualSense controller support (with haptics on Windows/Linux)
- **Debugging Tools**: ImGui-based debug UI, physics debug rendering, raycast visualization

## Technology Stack

- **Graphics**: Vulkan with GLFW windowing
- **Physics**: NVIDIA PhysX 5.x
- **Audio**: FMOD Studio
- **Math**: GLM (OpenGL Mathematics)
- **3D Asset Loading**: Assimp
- **GUI**: ImGui with ImGui implementation for Vulkan
- **Fonts**: FreeType (via ImGui)

## Getting Started: Basic Usage

Create an engine instance and load scenes:

```cpp
#include <engine.h>

int main() {
    Engine* engine = Engine::Create();
    engine->init(800, 600, "My Game");
    
    // Create scenes (inherit from Scene class)
    MyScene* scene = engine->createScene<MyScene>();
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

Customize scene behavior:

```cpp
class MyScene : public Scene {
    void EarlyInitScene(Engine* engine) override;
    void InitScene(Engine* engine) override;
    void UpdateScene(Engine* engine) override;
    void DestroyScene(Engine* engine) override;
};
```

Create game objects:

```cpp
// In InitScene or later
Mesh* mesh = engine->getMesh("myMesh");
Texture* texture = engine->getTexture("myTexture");
PhysicsMaterial* material = engine->createPhysicsMaterial(0.5f, 0.3f, 0.2f);

auto obj = engine->createGameObject<MyGameObject>(
    transform,
    mesh,
    texture,
    material,
    isDynamic  // true for dynamic physics, false for static
);
```

See [LabEscape Example](labescape_example.md) for a complete working game.
