# Physics

## PhysX Integration

VkEngine uses NVIDIA PhysX 4.x for physics simulation, collision detection, and character control. The physics system is tightly integrated with GameObjects and provides straightforward APIs for physics interactions.

## Physics Materials

### Creating Materials

```cpp
PhysicsMaterial* createPhysicsMaterial(
    float staticFriction,      // Friction when stationary
    float dynamicFriction,     // Friction when moving
    float restitution          // Bounciness (0-1, 1 = perfect bounce)
);
```

**Example**:
```cpp
// Slippery ice
PhysicsMaterial* ice = engine->createPhysicsMaterial(0.1f, 0.05f, 0.3f);

// Rough concrete
PhysicsMaterial* concrete = engine->createPhysicsMaterial(0.8f, 0.6f, 0.1f);

// Bouncy rubber
PhysicsMaterial* rubber = engine->createPhysicsMaterial(0.5f, 0.4f, 0.8f);

// Default material
PhysicsMaterial* standard = engine->createPhysicsMaterial(0.5f, 0.4f, 0.2f);
```

### Material Combinations

When two objects collide, their materials are combined:
- Friction = average of both materials
- Restitution = average of both materials

## Rigid Bodies in Game Objects

### Creating Objects with Physics

Physics bodies are automatically created when creating a GameObject with a mesh:

```cpp
createGameObject<T>(
    transform,
    mesh,
    texture,
    material,
    isDynamic  // Physics type determined by this flag
)
```

### Static Objects (isDynamic = false)

```cpp
Mesh* levelMesh = engine->getMesh("level");
PhysicsMaterial* mat = engine->createPhysicsMaterial(0.8f, 0.6f, 0.1f);

auto level = engine->createGameObject<GameObject>(
    {{0,0,0}, {0,0,0,1}, {1,1,1}},
    levelMesh,
    texture,
    mat,
    false  // Static - immovable collision geometry
);
```

**Properties**:
- Immovable and affected by no forces
- Uses triangle mesh collision (accurate for complex shapes)
- Good for terrain, buildings, static obstacles
- High performance (no simulation needed)

### Dynamic Objects (isDynamic = true)

```cpp
Mesh* ballMesh = engine->getMesh("ball");
PhysicsMaterial* bouncyMat = engine->createPhysicsMaterial(0.3f, 0.2f, 0.7f);

auto ball = engine->createGameObject<Ball>(
    {{0, 2, 0}, {0,0,0,1}, {1,1,1}},
    ballMesh,
    texture,
    bouncyMat,
    true  // Dynamic - simulated by physics engine
);
```

**Properties**:
- Affected by gravity and forces
- Continuous collision detection enabled (prevents tunneling)
- Fixed mass: 10.0 kg
- Angular damping: 1.0
- Linear damping: 0.5
- Uses convex mesh collision

## Applying Forces

### Impulse (Instantaneous Force)

```cpp
void applyForce(const Vector3& force);
// Applies force immediately, like an impulse or explosion
```

**Example**:
```cpp
void takeDamage(Vector3 explosionPoint) {
    // Knockback from explosion
    Vector3 knockback = (transform.position - explosionPoint).normalize() * 50.0f;
    applyForce(knockback);
}
```

### Directional Force

```cpp
void applyForce(Vector3 direction, float power);
// Applies normalized directional force
```

**Example**:
```cpp
void Update(Engine* engine) {
    // Wind force pushing object
    Vector3 windDirection = {1, 0, 0};
    float windStrength = 5.0f;
    applyForce(windDirection, windStrength);
}
```

## Querying Physics State

### Velocity

```cpp
Vector3 getVelocity();
// Returns current linear velocity
```

**Example**:
```cpp
void Update(Engine* engine) {
    Vector3 vel = getVelocity();
    float speed = sqrt(vel.x*vel.x + vel.y*vel.y + vel.z*vel.z);
    
    if (speed > maxSpeed) {
        // Moving too fast, apply drag
        applyForce(-getVelocity() * 0.5f);
    }
}
```

## Physics Types

### PhysicsType Enum

```cpp
enum class PhysicsType {
    Static,      // Immovable
    Kinematic,   // Scripted motion (e.g., platforms)
    Dynamic      // Physics-simulated
};
```

### Changing Physics Type at Runtime

```cpp
void setPhysicsType(PhysicsType type);
```

**Example**: Object falls when triggered

```cpp
void Update(Engine* engine) {
    if (shouldFall) {
        setPhysicsType(PhysicsType::Dynamic);
    }
}
```

## Collision Detection

### Collision Callbacks

```cpp
std::function<void(GameObject*, float)> onCollision;
```

Set a callback to be notified of collisions:

```cpp
void InitScene(Engine* engine) override {
    auto obj = engine->createGameObject<MyObject>(...);
    obj->onCollision = [this](GameObject* other, float impulse) {
        if (other->tag == "projectile") {
            takeDamage(impulse);
        }
    };
}
```

**Parameters**:
- `other`: The object this one collided with
- `impulse`: Magnitude of collision force

### Collision Layers

Currently no built-in collision layer system. To exclude collisions:
- Use Triggers instead of physics for certain interactions
- Manage collision logic in callbacks

## Triggers (Overlap Volumes)

Triggers are non-physical collision volumes that detect overlaps without affecting physics:

### Creating Triggers

```cpp
Trigger* createBoxTrigger(Vector3 position, Vector3 size);
```

**Example**:
```cpp
Trigger* damageZone = engine->createBoxTrigger(
    {0, 0, 10},  // Center position
    {5, 2, 5}    // Size (width, height, depth)
);

damageZone->onTriggerEnter = [this](GameObject* other) {
    if (other->tag == "player") {
        playerTakeDamage(10);  // Continuous damage in zone
    }
};

damageZone->onTriggerExit = [this](GameObject* other) {
    if (other->tag == "player") {
        stopDamage();
    }
};
```

### Callbacks

```cpp
std::function<void(GameObject*)> onTriggerEnter;
std::function<void(GameObject*)> onTriggerExit;
```

Called when objects enter/exit the trigger volume.

### Cleanup

```cpp
engine->requestDestroyTrigger(trigger);
```

## Physics Queries

### Raycasting

```cpp
RaycastHit raycast(Vector3 origin, Vector3 direction, float distance);

struct RaycastHit {
    float distance;        // Distance from origin to hit point
    GameObject* object;    // The object that was hit (null if no hit)
};
```

**Example**: Picking objects with mouse

```cpp
void MyScene::UpdateScene(Engine* engine) {
    if (engine->getMouseButton(MouseButton::Left) == PRESS) {
        Vector3 rayOrigin, rayDirection;
        engine->getMouseRay(rayOrigin, rayDirection);
        
        RaycastHit hit = engine->raycast(rayOrigin, rayDirection, 1000.0f);
        if (hit.object) {
            selectObject(hit.object);
        }
    }
}
```

**Ignores**:
- Trigger volumes
- CharacterController shapes (only solid objects)

### Sweep (AABB Overlap)

```cpp
SweepHit sweep(Vector3 position, Vector3 size, GameObject* ignore = nullptr);

struct SweepHit {
    std::vector<GameObject*> objects;  // All objects in volume
};
```

**Example**: Find nearby enemies

```cpp
Vector3 playerPos = player->transform.position;
SweepHit nearby = engine->sweep(playerPos, {10, 10, 10}, player);

for (GameObject* obj : nearby.objects) {
    if (obj->tag == "enemy") {
        engageEnemy(obj);
    }
}
```

## Character Controller

### Creation

The CharacterController is specialized for first-person character movement:

```cpp
ICharacterController* createCharacterController(
    float height,              // Capsule height (e.g., 1.8f)
    float radius,              // Capsule radius (e.g., 0.4f)
    Vector3 position,          // Starting position
    PhysicsMaterial* material, // Physics material
    bool interactWithActors    // true = push rigid bodies, false = pass through
);
```

### Movement

```cpp
void Move(Vector3 direction, float speed, float dt);
```

**Example**: Player movement in UpdateScene

```cpp
void PlayerScene::UpdateScene(Engine* engine) {
    Vector3 moveDir = {};
    if (engine->getKey(KeyCode::W) == PRESS) moveDir.z += 1;
    if (engine->getKey(KeyCode::S) == PRESS) moveDir.z -= 1;
    if (engine->getKey(KeyCode::A) == PRESS) moveDir.x -= 1;
    if (engine->getKey(KeyCode::D) == PRESS) moveDir.x += 1;
    
    controller->Move(moveDir, 10.0f, engine->getDeltaTime());
    
    // Update camera to follow controller
    Vector3 pos = controller->getPosition();
    engine->cameraPosition = pos + Vector3(0, 1.5f, 0);  // Eyes at 1.5m height
}
```

### Jumping

```cpp
void Jump(float force);
```

**Example**:
```cpp
void UpdateScene(Engine* engine) {
    if (engine->getKey(KeyCode::Space) == PRESS && isGrounded) {
        controller->Jump(15.0f);
    }
    
    // Check if still grounded
    float vertVel = controller->getVerticalVelocity();
    isGrounded = (vertVel == 0.0f);  // Grounded when vertical velocity is zero
}
```

### Position

```cpp
Vector3 getPosition();
void setPosition(Vector3 newPosition);
```

**Example**: Teleport or respawn
```cpp
if (fellOffMap) {
    controller->setPosition(spawnPoint);
}
```

### Vertical Velocity

```cpp
float getVerticalVelocity();
```

Useful for:
- Detecting if jumping or falling
- Animation state (falling, jumping, landing)
- Knockback recovery timing

**Example**: Animation state
```cpp
float vertVel = controller->getVerticalVelocity();
if (vertVel > 0) {
    setAnimationState("jumping");
} else if (vertVel < 0) {
    setAnimationState("falling");
} else {
    setAnimationState("idle");
}
```

### CharacterController Details

- **Gravity**: -24.0 units/s²
- **Shape**: Capsule (collision geometry)
- **Collision**: Stops at obstacles
- **Slope walking**: Automatically stays on slopes
- **Stepping**: Climbs small step heights
- **Mass**: Fixed for consistent feel

## Physics Coordinate System

- **X-axis**: Left/right (positive right)
- **Y-axis**: Front/back (positive back)
- **Z-axis**: Up/down (positive up, gravity is -Z)

This is a Z-up coordinate system.

## Physics Engine Details

### Simulation Accuracy

- **Time stepping**: Variable timestep per frame (uses engine delta time)
- **Solver iterations**: Configurable per simulation
- **Continuous collision detection**: Enabled for fast-moving objects
- **Sleeping**: Bodies sleep when inactive for performance

### Mesh Cooking

When a mesh is created:
- **Dynamic objects**: Converted to convex mesh (efficient, less accurate)
- **Static objects**: Converted to triangle mesh (accurate, static)
- **Caching**: Cooked meshes cached to avoid recomputation

### Performance Characteristics

- Dynamic bodies: O(n) where n = number of dynamic objects
- Static bodies: O(1) per-frame (shape is fixed)
- Triggers: O(n) overlap tests
- Raycasts: O(log n) with spatial acceleration

## Physics Implementation in LabEscape

LabEscape demonstrates:
- Player controlled by CharacterController
- Static level geometry from mesh
- Triggers for exit zones and puzzles
- Physics-based objects (balls in cup game)
- Collision-based interactions

## Tips and Best Practices

1. **Static for immovable**: Use static bodies for terrain, buildings
2. **Convex shapes optimal**: CharacterController and dynamic objects use convex meshes
3. **Avoid nested meshes**: One mesh per object for best performance
4. **Trigger for detection**: Use triggers instead of collision callbacks for non-physics events
5. **Raycast for picking**: Prefer raycasts over overlap tests for precise interaction
6. **Material tuning**: Test friction/restitution for intended feel
7. **Mass balance**: All dynamic objects have mass 10kg; adjust forces for balance
8. **Gravity direction**: Remember -Z is down; adjust camera accordingly

## Known Limitations

- No ragdoll physics
- No rope or cable simulation
- No destruction/deformable meshes
- No joint constraints (hinges, springs, etc.)
- No fluid simulation
- Limited collision layer system
- No sleeping optimization configuration
