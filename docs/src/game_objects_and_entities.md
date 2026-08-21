# Game Objects and Entities

## GameObject Class

The `GameObject` is the fundamental entity in VkEngine. All renderable things (players, enemies, props) are GameObjects. Developers create custom subclasses to add game-specific behavior.

### Core Properties

```cpp
class GameObject {
public:
    Transform transform;        // Position, rotation (quaternion), scale
    std::string name;           // For identification
    std::string tag;            // For categorization and filtering
    
    std::function<void(GameObject*, float)> onCollision;  // Collision callback
};
```

**Transform Details**:
- `position`: World space location (Vector3)
- `rotation`: Quaternion representing orientation
- `scale`: Object size scaling (Vector3)

All transforms are updated via `updateTransform()` when modified.

### Lifecycle

#### Creation
```cpp
Mesh* mesh = engine->getMesh("cube");
Texture* tex = engine->getTexture("white");
PhysicsMaterial* mat = engine->createPhysicsMaterial(0.5f, 0.3f, 0.2f);

auto obj = engine->createGameObject<MyObject>(
    transform,      // Initial transform
    mesh,           // Render geometry
    tex,            // Surface appearance
    mat,            // Physics material
    isDynamic       // true for physics simulation, false for static
);
// Engine calls obj->Start() automatically
```

#### Lifecycle Hooks
```cpp
virtual void Start(Engine* engine);     // Called after creation
virtual void Update(Engine* engine);    // Called every frame
virtual void Destroy(Engine* engine);   // Called on cleanup
```

Example custom subclass:

```cpp
class Enemy : public GameObject {
public:
    void Start(Engine* engine) override {
        // Initialization: load resources, set physics, attach sounds
        walkSound = engine->getSound("walk_sfx");
    }
    
    void Update(Engine* engine) override {
        // Per-frame logic: move, animate, detect player proximity
        transform.position += velocity * engine->getDeltaTime();
    }
    
    void Destroy(Engine* engine) override {
        // Cleanup (though most cleanup is automatic)
    }
    
private:
    Sound* walkSound;
    Vector3 velocity;
};
```

### Physics Integration

#### Static vs Dynamic

```cpp
// Static object (part of environment)
engine->createGameObject<Rock>(
    transform, mesh, texture, material,
    false  // Static - doesn't move, doesn't respond to physics
);

// Dynamic object (affected by gravity and collisions)
engine->createGameObject<Ball>(
    transform, mesh, texture, material,
    true   // Dynamic - simulated by PhysX
);
```

#### Applying Forces

```cpp
void Update(Engine* engine) override {
    // Apply directional force
    applyForce(Vector3 direction, float power);
    
    // Apply specific force vector
    applyForce(Vector3 force);
    
    // Query current velocity
    Vector3 vel = getVelocity();
}
```

#### Collision Callbacks

```cpp
obj->onCollision = [this](GameObject* other, float impulse) {
    // Called when this object collides with 'other'
    // impulse = magnitude of collision force
    
    if (other->tag == "enemy") {
        health -= 10;
    }
};
```

#### Changing Physics at Runtime

```cpp
void Update(Engine* engine) override {
    if (shouldFall) {
        setPhysicsType(PhysicsType::Dynamic);  // Now affected by gravity
    }
}
```

### Audio Integration

#### Playing Sounds

```cpp
void Update(Engine* engine) override {
    if (isMoving) {
        // Play sound at object's location with spatial audio
        Sound* footstep = engine->getSound("footstep");
        playSound(footstep, 1.0f);  // volume = 1.0
    }
}
```

#### Sound Control

```cpp
void Update(Engine* engine) override {
    if (isPaused) {
        setSoundPause(true);  // Pause all sounds from this object
    }
    
    if (shouldStopAll) {
        stopAllSounds();      // Stop all active sounds
    }
}
```

**Spatial Positioning**: Sound position automatically follows object transform. 3D sounds have distance-based attenuation configured at creation time.

### Rendering

#### Updating Appearance

```cpp
void Update(Engine* engine) override {
    if (takeDamage) {
        Texture* damagedTex = engine->getTexture("rock_damaged");
        updateTexture(damagedTex);  // Change surface appearance
    }
}
```

#### Getting ID

```cpp
uint32_t id = getID();  // Unique ID within engine session
```

## CharacterController

A specialized kinematic physics entity for player movement. Unlike GameObjects, CharacterControllers use PhysX's kinematic character controller for reliable first-person mechanics.

### Creation

```cpp
ICharacterController* controller = engine->createCharacterController(
    height,                 // 1.8f typical for human
    radius,                 // 0.4f typical for human width
    position,               // Initial spawn point
    material,               // Physics material
    interactWithActors      // true = interact with dynamic objects, false = pass through
);
```

### Movement

```cpp
// Primary movement interface
void Move(Vector3 direction, float speed, float dt);
// direction = normalized direction vector (forward/back/left/right)
// speed = units per second
// dt = delta time from engine

// Example: WASD input
Vector3 dir = Vector3(0, 0, 0);
if (engine->getKey(KeyCode::W) == PRESS) dir.z += 1;
if (engine->getKey(KeyCode::S) == PRESS) dir.z -= 1;
if (engine->getKey(KeyCode::A) == PRESS) dir.x -= 1;
if (engine->getKey(KeyCode::D) == PRESS) dir.x += 1;

controller->Move(dir, 10.0f, engine->getDeltaTime());
```

### Jumping

```cpp
void Jump(float force);

// Typical usage
if (engine->getKey(KeyCode::Space) == PRESS && isGrounded) {
    controller->Jump(15.0f);  // force = upward impulse
}
```

### State Queries

```cpp
Vector3 pos = controller->getPosition();
controller->setPosition(newPos);            // Teleport

float vertVel = controller->getVerticalVelocity();
if (vertVel < 0) isGrounded = false;
if (vertVel == 0) isGrounded = true;
```

### Characteristics

- Built on PhysX kinematic controller (not dynamic rigid body)
- Automatic gravity application and vertical velocity accumulation
- Automatically handles slope walking and step climbing
- Smooth, reliable first-person camera control
- Can be configured to interact or ignore dynamic objects

## Triggers

Non-rendered physics volumes that detect overlaps with GameObjects. Unlike colliders, triggers don't affect physics simulation.

### Creation

```cpp
Trigger* exitZone = engine->createBoxTrigger(
    position,       // Center position
    size            // Box dimensions (width, height, depth)
);
```

### Callbacks

```cpp
exitZone->onTriggerEnter = [this](GameObject* other) {
    if (other->tag == "player") {
        levelComplete = true;
    }
};

exitZone->onTriggerExit = [this](GameObject* other) {
    if (other->tag == "player") {
        levelComplete = false;
    }
};
```

### Cleanup

```cpp
engine->requestDestroyTrigger(exitZone);
```

### Use Cases

- Level exit zones
- Pickup areas
- Hazard detection
- Cutscene triggers
- Spawn zones
- Environmental effects (water, lava)
