# Scenes

## Scene System

Scenes are the primary organizational unit in VkEngine. They contain game objects and resources for a logical level or area of your game. Each scene is responsible for its own initialization, updates, and cleanup. Developers create custom scene classes to implement game-specific logic.

## Core Scene Class

Base class for creating custom scenes:

```cpp
class Scene {
public:
    virtual ~Scene() {}
    
    virtual void EarlyInitScene(Engine* engine);     // Before resource loading
    virtual void InitScene(Engine* engine);          // After resources loaded
    virtual void UpdateScene(Engine* engine);        // Per-frame updates
    virtual void DestroyScene(Engine* engine);       // Cleanup
    
    virtual GameObject* CreateGameObject(
        Engine* engine, const char* objectType, const char* tag,
        const char* name, Transform transform,
        Mesh* mesh, Texture* texture, bool dynamic);
    
    virtual void ResourceLoaded(std::string name, const char* path, ResourceType type);
};
```

## Initialization Phases

### EarlyInitScene

Called first, **before** resources are loaded. Use this phase to:
- Request resources to be loaded
- Configure scene parameters
- Initialize systems

```cpp
class Level1Scene : public Scene {
    void EarlyInitScene(Engine* engine) override {
        // Request resources (they may not be ready yet)
        // This is where you'd start background loading tasks
        
        // Initialize RNG for procedural generation
        rng.seed(time(nullptr));
    }
};
```

### InitScene

Called **after** all requested resources are available. Use this phase to:
- Create game objects using loaded resources
- Set up physics triggers
- Configure scene-specific audio
- Initialize game state

```cpp
class Level1Scene : public Scene {
    void InitScene(Engine* engine) override {
        // All resources are now available
        Mesh* playerMesh = engine->getMesh("player");
        Texture* playerTex = engine->getTexture("player");
        PhysicsMaterial* mat = engine->createPhysicsMaterial(0.5f, 0.3f, 0.2f);
        
        // Create player
        auto player = engine->createGameObject<Player>(
            {0, 1, 0, {0,0,0,1}, {1,1,1}},  // Transform
            playerMesh,
            playerTex,
            mat,
            true  // Dynamic
        );
        
        // Create level environment
        Mesh* levelMesh = engine->getMesh("level");
        auto level = engine->createGameObject<GameObject>(
            {0, 0, 0, {0,0,0,1}, {1,1,1}},
            levelMesh,
            nullptr,  // No texture
            mat,
            false  // Static
        );
        
        // Set up exit trigger
        exitTrigger = engine->createBoxTrigger({0, 1, 10}, {2, 2, 2});
        exitTrigger->onTriggerEnter = [this](GameObject* other) {
            if (other->tag == "player") levelComplete = true;
        };
    }
};
```

## Runtime Behavior

### UpdateScene

Called every frame for scene-specific logic:

```cpp
class Level1Scene : public Scene {
    void UpdateScene(Engine* engine) override {
        // Check for level completion
        if (levelComplete) {
            loadNewScene = true;
            sceneToLoad = nextLevel;
        }
        
        // Update UI
        updateHUD(engine);
        
        // Play ambient sounds
        if (ambienceSound && !isPlaying) {
            player->playSound(ambienceSound, 0.3f);
            isPlaying = true;
        }
    }
};
```

### DestroyScene

Called during cleanup. Usually most cleanup is automatic, but use this for:
- Stopping background tasks
- Saving game state
- Explicit cleanup for complex objects

```cpp
class Level1Scene : public Scene {
    void DestroyScene(Engine* engine) override {
        // Stop background music
        if (musicPlaying) {
            engine->requestDestroy(musicSound);
        }
        
        // Save any persistent game state
        savePlayerProgress();
    }
};
```

## Object Creation Customization

Override `CreateGameObject()` to customize object instantiation:

```cpp
GameObject* CreateGameObject(
    Engine* engine, 
    const char* objectType,    // Custom type name from scene file
    const char* tag,
    const char* name,
    Transform transform,
    Mesh* mesh,
    Texture* texture,
    bool dynamic
) override {
    // Create custom game object types based on objectType string
    if (strcmp(objectType, "enemy") == 0) {
        return engine->createGameObject<Enemy>(transform, mesh, texture, material, dynamic);
    }
    else if (strcmp(objectType, "pickup") == 0) {
        return engine->createGameObject<Pickup>(transform, mesh, texture, material, dynamic);
    }
    else if (strcmp(objectType, "trigger_zone") == 0) {
        return engine->createGameObject<TriggerZone>(transform, mesh, texture, material, dynamic);
    }
    
    // Default: standard GameObject
    return engine->createGameObject<GameObject>(transform, mesh, texture, material, dynamic);
}
```

## Scene Resources

Scenes manage collections of:
- **Meshes**: 3D geometry available in the scene
- **Textures**: Image assets available in the scene
- **Game Objects**: Instantiated entities in the scene

These are automatically organized for efficient rendering and physics.

## Scene File Format

Scene files define the initial layout. Format is text-based:

```
MESH: meshName filePath
TEXTURE: textureName filePath
OBJECT: objectType tag name meshName textureName dynamic
TRANSFORM: posX posY posZ rotX rotY rotZ rotW scaleX scaleY scaleZ
```

**Example scene file**:
```
MESH: player models/player.obj
MESH: level models/level.obj
TEXTURE: player_tex textures/player.png
TEXTURE: level_tex textures/level.png

OBJECT: Player player Player player_mesh player_tex true
TRANSFORM: 0 1 0  0 0 0 1  1 1 1

OBJECT: LevelGeometry level Level level_mesh level_tex false
TRANSFORM: 0 0 0  0 0 0 1  1 1 1
```

**Loading from file**:
```cpp
bool valid;
MyScene* scene = engine->createScene<MyScene>("assets/level1.scene", &valid);
if (!valid) {
    std::cerr << "Failed to load scene file\n";
}
engine->loadScene(scene);
```

**Creating empty scene**:
```cpp
MyScene* scene = engine->createScene<MyScene>();
engine->loadScene(scene);
```

## Scene Transitions

**In main game loop**:
```cpp
while (engine->running()) {
    engine->updateScene();
    engine->update();
    engine->render();
    
    // Check if scene wants to transition
    if (engine->isLastFrame()) {
        auto scene = static_cast<MyScene*>(engine->getActiveScene());
        if (scene && scene->loadNewScene) {
            scene->loadNewScene = false;
            engine->loadScene(scene->sceneToLoad);
        }
    }
}
```

**In scene class**:
```cpp
class Level1Scene : public Scene {
public:
    bool loadNewScene = false;
    Scene* sceneToLoad = nullptr;
    
    void UpdateScene(Engine* engine) override {
        if (playerReachedExit) {
            loadNewScene = true;
            sceneToLoad = nextLevel;
        }
    }
};
```

## Example: Complete Game Scene

```cpp
class GameLevel : public Scene {
private:
    ICharacterController* player;
    Sound* ambience;
    Sound* footsteps;
    bool levelComplete = false;
    
public:
    void EarlyInitScene(Engine* engine) override {
        // Create character controller
        PhysicsMaterial* mat = engine->createPhysicsMaterial(0.5f, 0.3f, 0.2f);
        player = engine->createCharacterController(1.8f, 0.4f, {0, 1, 0}, mat, true);
    }
    
    void InitScene(Engine* engine) override {
        // Load assets
        ambience = engine->createSound("ambient", "assets/wind.ogg", true, false);
        footsteps = engine->createSound("step", "assets/footstep.ogg", false, true);
        
        Mesh* levelMesh = engine->getMesh("level");
        auto levelGeo = engine->createGameObject<GameObject>(
            {{0,0,0}, {0,0,0,1}, {1,1,1}},
            levelMesh, nullptr, mat, false
        );
        
        // Create exit trigger
        Trigger* exit = engine->createBoxTrigger({0, 1, 10}, {2, 2, 2});
        exit->onTriggerEnter = [this](GameObject* other) {
            levelComplete = true;
        };
    }
    
    void UpdateScene(Engine* engine) override {
        // Handle player movement
        Vector3 moveDir = {};
        if (engine->getKey(KeyCode::W) == PRESS) moveDir.z += 1;
        if (engine->getKey(KeyCode::S) == PRESS) moveDir.z -= 1;
        if (engine->getKey(KeyCode::A) == PRESS) moveDir.x -= 1;
        if (engine->getKey(KeyCode::D) == PRESS) moveDir.x += 1;
        
        player->Move(moveDir, 10.0f, engine->getDeltaTime());
        
        // Check level completion
        if (levelComplete) {
            loadNewScene = true;
            sceneToLoad = nextLevel;
        }
    }
    
    void DestroyScene(Engine* engine) override {
        // Cleanup happens automatically
    }
};
```

## Tips and Best Practices

1. **Separate concerns**: Keep scene initialization clean, move complex logic to GameObjects
2. **Use tags for filtering**: Filter objects by tag for quick lookups and logic
3. **Scene files for layout**: Use scene files for static level design, code for dynamic behavior
4. **Resource sharing**: Load shared resources once, reuse across objects
5. **Deferred cleanup**: Always use `requestDestroy()`, never delete directly
6. **Camera control**: Engine camera follows the physics, override for custom behavior
