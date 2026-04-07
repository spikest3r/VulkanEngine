# 🌌 VulkanEngine v1 (Beta)

![Vulkan](https://img.shields.io/badge/API-Vulkan%201.3-AC162C?style=for-the-badge&logo=vulkan&logoColor=white)
![PhysX](https://img.shields.io/badge/Physics-NVIDIA%20PhysX-76B900?style=for-the-badge&logo=nvidia&logoColor=white)
![FMOD](https://img.shields.io/badge/Audio-FMOD-00AEEF?style=for-the-badge&logo=fmod&logoColor=white)

**VulkanEngine** is a high-performance C++ engine core built around a **static factory architecture**, custom memory allocator, GPU-driven rendering pipeline (Vulkan 1.3), PhysX physics layer, and FMOD spatial audio system.

Work in progress.

---

# Core Lifecycle

The engine is instantiated via a **static factory** to ensure correct memory ownership across the DLL boundary.

    #include "Engine.h"

Included Editor (beta) and TestGame (use to try engine features and compilation success).

---

# Core Lifecycle

The engine is instantiated via a **static factory** to ensure correct memory ownership across the DLL boundary.

    #include "Engine.h"

    int main() {
        Engine* engine = Engine::Create();

        engine->init(1920, 1080, "VulkanEngine v0.2 Beta");

        while (engine->running()) {
            engine->update();
        
        float dt = engine->getDeltaTime();

            engine->render();
    }

        engine->cleanup();
        Engine::Destroy(engine);

        return 0;
    }

---

# Engine Core System

## System Control

| Function | Description |
|------|------|
| `init(w, h, title)` | Initializes window, renderer, physics, audio |
| `update()` | Updates input, physics, timers, scene logic |
| `render()` | Submits render commands |
| `running()` | Main loop condition |
| `cleanup()` | Destroys GPU/CPU resources safely |
| `exit()` | Requests shutdown |
| `getDeltaTime()` | Frame delta time |

---

# Input System

## Keyboard & Mouse

    KeyState state = engine->getKey(KeyCode::W);
    KeyState mouse = engine->getMouseButton(MouseButton::Left);
    Vector2 pos = engine->getMousePos();

Supports:
- Keyboard
- Mouse buttons
- Cursor position tracking

---

# Camera System

The engine exposes a global camera state:

    Vector3 cameraPosition;
    Vector3 cameraRotation;
    Vector3 cameraOffset;
    NearFarPlanes planes;

## Utilities

    Vector3 forward, right;
    engine->getCameraVectors(forward, right);

Default behavior:
- Yaw/Pitch rotation
- FPS-style vectors
- Manual override supported

---

# Scene System

Scenes are first-class runtime objects.

## Loading / Unloading

    engine->loadScene(scene);
    engine->unloadActiveScene();
    Scene* active = engine->getActiveScene();

## Scene Creation

Runtime scene:

    Scene* scene = engine->createScene<MyScene>();

File-based:

    bool valid = false;
    Scene* scene = engine->createScene<MyScene>("level01.scene", &valid);

---

# GameObject System

## Create GameObject

    GameObject* obj = engine->createGameObject<MyObject>(
        transform,
        mesh,
        texture,
        physicsMaterial,
        true
    );

---

# Physics System

## Character Controller

    ICharacterController* controller =
        engine->createCharacterController(
            1.8f,
            0.3f,
            position,
            material,
            true
        );

## Physics Materials

    PhysicsMaterial* mat = engine->createPhysicsMaterial(
        0.5f,
        0.4f,
        0.1f
    );

## Triggers

    Trigger* trigger = engine->createBoxTrigger(pos, size);

## Raycasting

    RaycastHit hit = engine->raycast(origin, direction, 100.0f);

---

# Audio System

    Sound* sfx = engine->createSound("explosion", "boom.wav", false, true);
    engine->setGlobalMute(true);

## DualSense haptics (```dualsense``` branch)

    engine->playHaptics(sfx, 0.8f);

---

# Resource System

    Texture* tex = engine->createTexture("brick", "brick.png");
    Mesh* mesh = engine->createMesh("cube", "cube.obj");
    Sound* snd = engine->createSound("click", "click.wav", false, false);

---

# Memory System

    void* ptr = Engine::requestMemory(size);
    Engine::freeMemory(ptr);

---

# Safe Destruction

    engine->requestDestroy(resource);
    engine->requestDestroyGameObject(object);
    engine->requestDestroyTrigger(trigger);
    engine->requestDestroyCharacterController(ctrl);
    engine->requestDestroyScene(scene);

---

# Utilities

    engine->addTimer(2.0f, []() {
        std::cout << "Timer elapsed\n";
    });

    engine->setCursorMode(CursorMode::Locked);

    engine->SetUICallback([]() {
        // UI layer
    });

---

# Rendering Controls

    engine->setClearColor({0.1f, 0.1f, 0.15f});

---

# Critical Engine Rules

> [!CAUTION]

1. No raw delete  
Everything must go through requestDestroy()

2. Scene ownership is strict  
Only one active scene unless swapped explicitly

---

# Todo

- Make rendering more efficient (better descriptor set management)
- 2D Image UI