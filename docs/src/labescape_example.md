# LabEscape Example Game

## Overview

LabEscape is a complete puzzle-escape game built with VkEngine. It demonstrates all major engine features in a production game context including rendering, physics, audio, input, scene management, and game logic. The source code serves as the primary example for VkEngine usage patterns.

Github Repo: [https://github.com/spikest3r/LabEscape_VkEngine](https://github.com/spikest3r/LabEscape_VkEngine)

## Game Structure

### Main Loop

```cpp
int main() {
    Engine* engine = Engine::Create();
    engine->init(800, 600, "Lab Escape");
    
    // Initialize physics material for player
    GlobalObjects::characterMaterial = engine->createPhysicsMaterial(0.0f, 0.0f, 0.0f);
    
    // Load fonts for UI
    ToolUI::AddFontFromFileTTF(UIFonts::defaultFont, fontPath, 14.0f);
    ToolUI::AddFontFromFileTTF(UIFonts::largeFont, fontPath, 28.0f);
    
    // Create all scenes
    GlobalObjects::level1 = engine->createScene<Level1Scene>("assets/level1.scene", &valid);
    GlobalObjects::level2 = engine->createScene<Level2Scene>("assets/level2.scene", &valid);
    GlobalObjects::level3 = engine->createScene<Level3Scene>("assets/level3.scene", &valid);
    GlobalObjects::levelFinal = engine->createScene<LevelFinalScene>("assets/level4.scene", &valid);
    GlobalObjects::intro = engine->createScene<IntroScene>();
    GlobalObjects::credits = engine->createScene<CreditsScene>();
    
    // Start at intro
    engine->setCursorMode(CursorMode::DISABLED);
    engine->loadScene(GlobalObjects::intro);
    
    // Main loop
    while (engine->running()) {
        engine->updateScene();
        engine->update();
        engine->render();
        
        // Handle scene transitions
        if (engine->isLastFrame()) {
            auto scene = static_cast<BaseScene*>(engine->getActiveScene());
            if (scene && scene->loadNewScene) {
                scene->loadNewScene = false;
                engine->loadScene(scene->sceneToLoad);
            }
        }
    }
    
    engine->cleanup();
    Engine::Destroy(engine);
    return 0;
}
```

## Scene Hierarchy

### BaseScene

Base class for all game scenes:

```cpp
class BaseScene : public Scene {
public:
    bool loadNewScene = false;
    Scene* sceneToLoad = nullptr;
};
```

Provides scene transition mechanics.

### PlayerScene

Base class for playable levels:

```cpp
class PlayerScene : public BaseScene {
private:
    ICharacterController* controller;
    Sound* step1;
    Sound* step2;
    Trigger* exitTrigger;
    
    // Movement settings
    float baseMoveSpeed = 12.0f;
    float mouseSensitivity = 0.1f;
    
    // Camera bobbing
    float bobbingAmplitude = 0.2f;
    float bobbingFrequency = 13.0f;
    
protected:
    void InitScene(Engine* engine) override;
    void UpdateScene(Engine* engine) override;
    void checkKeyboard(Engine* engine);
    void checkGamepad(Engine* engine);
    void processMovement(Engine* engine);
};
```

**Features**:
- Character controller for player movement
- WASD/gamepad movement input
- Mouse/right stick camera control
- Footstep sound effects
- Camera head bobbing animation
- Exit trigger for level completion

### Level Scenes

#### Level1Scene
Introduction level with basic mechanics.

#### Level2Scene
Intermediate puzzles.

#### Level3Scene
Advanced mechanics with multiple puzzle systems:
- Simon Says memory puzzle
- Ball-in-cup game
- Password keypad system

#### LevelFinalScene
Final escape sequence.

### IntroScene & CreditsScene

Menu scenes for game flow.

## Game Objects Used in LabEscape

### Player Character

Implemented as `CharacterController`:

```cpp
void PlayerScene::InitScene(Engine* engine) {
    controller = engine->createCharacterController(
        1.8f,  // Height
        0.4f,  // Radius
        spawnPosition,
        globalMaterial,
        true   // Interact with actors
    );
}
```

**Features**:
- First-person perspective
- WASD movement or gamepad stick
- Mouse look or gamepad right stick
- Gravity-based jumping with jump animation
- Head bobbing while moving
- Footstep sounds on movement

### Interactive Objects

**Keypad** - Numerical input puzzle:
```cpp
Keypad* keypad = engine->createGameObject<Keypad>(
    transform, mesh, texture, material, false
);
keypad->setCode("1234");
keypad->onCodeEntered = [this](bool correct) {
    if (correct) doorOpen = true;
};
```

**Simon Says Cubes** - Memory puzzle:
```cpp
SimonSaysCube* cube = engine->createGameObject<SimonSaysCube>(
    transform, mesh, texture, material, false
);
cube->onClicked = [this](int index) {
    // Process player input for Simon Says game
};
```

**Notes** - Readable objects:
```cpp
NoteObject* note = engine->createGameObject<NoteObject>(
    transform, nullptr, noteTexture, nullptr, false
);
note->text = "Important clue...";
note->onRead = [this]() { unlockedHint = true; };
```

### Environmental Objects

**Doors** - Animated objects:
```cpp
GameObject* door = engine->createGameObject<GameObject>(
    transform, doorMesh, doorTexture, material, false
);
// Animated via transform updates in UpdateScene
```

**Level Geometry** - Static collision:
```cpp
auto levelGeo = engine->createGameObject<GameObject>(
    {{0,0,0}, identity, {1,1,1}},
    levelMesh,
    levelTexture,
    groundMaterial,
    false  // Static
);
```

## Real Usage Examples

### Movement and Camera Control

From `PlayerScene::UpdateScene()`:

```cpp
void PlayerScene::UpdateScene(Engine* engine) {
    // Get movement input
    Vector3 moveDir = {};
    if (engine->getKey(KeyCode::W) == PRESS) moveDir.z += 1;
    if (engine->getKey(KeyCode::S) == PRESS) moveDir.z -= 1;
    if (engine->getKey(KeyCode::A) == PRESS) moveDir.x -= 1;
    if (engine->getKey(KeyCode::D) == PRESS) moveDir.x += 1;
    
    // Apply movement
    controller->Move(moveDir, baseMoveSpeed, engine->getDeltaTime());
    
    // Update camera
    Vector3 pos = controller->getPosition();
    engine->cameraPosition = pos + Vector3(0, 1.6f, 0);  // Eye height
    
    // Handle mouse look
    Vector2 mousePos = engine->getMousePos();
    if (firstMouse) {
        lastX = mousePos.x;
        lastY = mousePos.y;
        firstMouse = false;
    }
    
    float xOffset = mousePos.x - lastX;
    float yOffset = lastY - mousePos.y;  // Reversed Y
    lastX = mousePos.x;
    lastY = mousePos.y;
    
    yaw += xOffset * mouseSensitivity;
    pitch += yOffset * mouseSensitivity;
    
    // Clamp pitch
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    
    engine->cameraRotation = {pitch, yaw, 0};
}
```

### Raycast-Based Interaction

From level update logic:

```cpp
void PlayerScene::checkRaycast(Engine* engine) {
    // Create ray from camera
    Vector3 forward, right;
    engine->getCameraVectors(forward, right);
    
    Vector3 rayOrigin = engine->cameraPosition;
    Vector3 rayDirection = forward;
    
    // Raycast in front of player
    RaycastHit hit = engine->raycast(rayOrigin, rayDirection, 3.0f);
    
    if (hit.object) {
        if (hit.object->tag == "interactive") {
            // Highlight interactable
            if (engine->getKey(KeyCode::E) == PRESS) {
                hit.object->onInteract();
            }
        }
    }
}
```

### Trigger-Based Puzzle Logic

From Level3Scene:

```cpp
void Level3Scene::InitScene(Engine* engine) {
    // Simon Says puzzle trigger
    simonSaysTrigger = engine->createBoxTrigger(
        simonSaysTablePos, 
        {4.0f, 5.0f, 10.0f}
    );
    
    simonSaysTrigger->onTriggerEnter = [this](GameObject* other) {
        if (other->tag == "player" && !simonSaysRunning) {
            startSimonSaysGame();
        }
    };
}
```

### Sound Management

From PlayerScene:

```cpp
void PlayerScene::InitScene(Engine* engine) {
    // Load footstep sounds
    step1 = engine->createSound("step1", "assets/footstep1.wav", false, true);
    step2 = engine->createSound("step2", "assets/footstep2.wav", false, true);
    
    // Ambient sound
    ambientSfx = engine->createSound("ambient", "assets/wind.ogg", true, false);
    ambientPlayer = engine->createGameObject<GameObject>(
        {{0,0,0}, identity, {1,1,1}},
        nullptr, nullptr, nullptr, false
    );
}

void PlayerScene::processMovement(Engine* engine) {
    if (isMoving && !stepPlaying) {
        // Play alternating footsteps
        Sound* step = (std::rand() % 2 == 0) ? step1 : step2;
        ambientPlayer->playSound(step, 0.7f);
        stepPlaying = true;
        stepTimer = stepInterval;
    }
}
```

### DualSense Features (Windows)

From Level3Scene:

```cpp
void Level3Scene::InitScene(Engine* engine) {
    // Set idle lightbar color
    if (engine->isDualSenseAttached()) {
        engine->dualsense_setLightbarColor(190, 200, 255);  // Cyan
    }
}

void PlayerScene::UpdateScene(Engine* engine) {
    // Change color based on puzzle state
    if (puzzleSolved) {
        engine->dualsense_setLightbarColor(0, 255, 0);  // Green
    } else if (wrongAttempt) {
        engine->dualsense_setLightbarColor(255, 0, 0);  // Red
        
        // Haptic feedback on error
        Sound* errorSound = engine->getSound("error_sfx");
        engine->dualsense_playHaptics(errorSound, 1.0f);
    }
}
```

### UI Integration with ImGui

From `SetUICallback`:

```cpp
engine->SetUICallback([](Engine* engine) {
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Camera Pos: %.2f, %.2f, %.2f", 
        engine->cameraPosition.x,
        engine->cameraPosition.y,
        engine->cameraPosition.z);
    ImGui::End();
});
```

### Scene File Example (level1.scene)

```
MESH: level models/level1.obj
MESH: door models/door.obj
TEXTURE: level_floor textures/floor.png
TEXTURE: door_frame textures/door.png

OBJECT: LevelGeometry level level level_floor false
TRANSFORM: 0 0 0  0 0 0 1  1 1 1

OBJECT: Door door Door door_frame true
TRANSFORM: 0 2 10  0 0 0 1  1 1 1
```

## Key Design Patterns Used

### Global State Management

```cpp
struct GlobalObjects {
    static PhysicsMaterial* characterMaterial;
    static Level1Scene* level1;
    static Level2Scene* level2;
    static Level3Scene* level3;
    static LevelFinalScene* levelFinal;
    static IntroScene* intro;
    static CreditsScene* credits;
};
```

Allows levels to reference each other for transitions.

### Scene Transition Pattern

```cpp
void Level1Scene::UpdateScene(Engine* engine) {
    if (playerReachedExit) {
        loadNewScene = true;
        sceneToLoad = GlobalObjects::level2;
    }
}
```

Main loop handles actual transition on next frame.

### Input Filtering

```cpp
bool pauseKeyPressed = false;

void UpdateScene(Engine* engine) {
    bool pauseKeyDown = (engine->getKey(KeyCode::Escape) == PRESS);
    
    if (pauseKeyDown && !pauseKeyPressed) {
        togglePause();
        pauseKeyPressed = true;
    }
    
    if (!pauseKeyDown) {
        pauseKeyPressed = false;
    }
}
```

Prevents repeated actions from held keys.

### Deferred Destruction

```cpp
void Level1Scene::UpdateScene(Engine* engine) {
    if (shouldRemoveObject) {
        engine->requestDestroyGameObject(object);
        // Object still valid, destroyed later
    }
}
```

Safe cleanup without iterator invalidation.

## Learning Resources

To understand VkEngine better, study LabEscape:

1. **main.cpp** - Main loop and initialization
2. **basescene.h** - Base scene implementation
3. **levelscenes.h** - Level-specific scenes
4. **level*.cpp** - Detailed game logic for each level
5. **Assets** - See how resources are organized and referenced

## Performance Characteristics

LabEscape runs smoothly on:
- **Windows 10/11** with modern GPU (GTX 1060 or better)
- **Linux** with Vulkan-capable GPU

Typical performance:
- **FPS**: 60 with v-sync
- **Memory**: 200-300 MB
- **VRAM**: 100-150 MB

## Extending LabEscape

To create your own game using LabEscape as a template:

1. **Copy project structure**
2. **Replace scenes** - Create your own Scene classes
3. **Add custom GameObjects** - Inherit from GameObject for specific types
4. **Create assets** - Models, textures, sounds
5. **Wire up input** - Customize PlayerScene input handling
6. **Implement puzzles** - Use triggers and raycasts

LabEscape demonstrates all necessary patterns; your game just needs to customize the specifics.
