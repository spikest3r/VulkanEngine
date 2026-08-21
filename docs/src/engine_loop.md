# Engine Loop

## Main Game Loop

The engine loop is the heartbeat of the game, executing once per frame and coordinating all systems.

## Basic Loop Structure

```cpp
int main() {
    Engine* engine = Engine::Create();
    engine->init(800, 600, "My Game");
    
    MyScene* scene = engine->createScene<MyScene>();
    engine->loadScene(scene);
    
    // Main game loop
    while (engine->running()) {
        engine->updateScene();    // Update scene logic
        engine->update();         // Update physics, input, timers, audio
        engine->render();         // Render frame with Vulkan
    }
    
    engine->cleanup();
    Engine::Destroy(engine);
    return 0;
}
```

## Loop Phases

### 1. UpdateScene (Game Logic)

```cpp
engine->updateScene();
```

Calls the active scene's `UpdateScene()` method:

```cpp
void Scene::UpdateScene(Engine* engine) {
    // Per-frame game logic
    // Update HUD, check level completion, manage enemies, etc.
}
```

**Responsibilities**:
- Update scene-specific logic
- Manage level progression
- Control ambient effects
- Handle scene transitions

### 2. Update (Engine Systems)

```cpp
engine->update();
```

Coordinates all engine subsystems in order:

**2.1 Input Polling**:
- `glfwPollEvents()` - Get keyboard, mouse, window events
- Gamepad state updated
- Cursor position recorded
- Mouse scroll delta processed

**2.2 Timing**:
- Calculate elapsed time since last frame
- Update delta time
- Used by CharacterController, physics, and user code

**2.3 Game Object Updates**:
```cpp
forEach (GameObject in active scene) {
    gameObject->Update(engine);  // User-defined per-object logic
}
```

Each GameObject's Update() is called:
- Input handling
- Local state updates
- Sound playback
- Movement logic

**2.4 Physics Step**:
```cpp
physicsScene->simulate(deltaTime);  // PhysX simulation
physicsScene->fetchResults();       // Get collision results
```

Simulation:
- Apply gravity
- Update velocities
- Detect collisions
- Apply collision responses
- Update rigid body positions

**2.5 Collision Processing**:
- Process collision callbacks (onCollision)
- Process trigger callbacks (onTriggerEnter/onTriggerExit)
- Update CharacterController position

**2.6 Transform Synchronization**:
- Update Vulkan model matrices from GameObject transforms
- Update PhysX actor positions from GameObjects
- Sync CharacterController position to camera

**2.7 Camera Update**:
- Calculate view matrix from `cameraPosition` and `cameraRotation`
- Calculate projection matrix from aspect ratio and FOV
- Update view frustum for rendering

**2.8 Audio System Update**:
- Update FMOD system (`fmodSystem->update()`)
- Sync 3D listener to camera position
- Update all 3D sound source positions
- Process audio playback and effects

**2.9 Timer Processing**:
```cpp
oneShotTimers.update(currentTime);  // Process event timers
```

Execute callbacks for timers that have elapsed:
```cpp
engine->addTimer(2.5f, []() {
    std::cout << "2.5 seconds have passed\n";
});
```

**2.10 Resource Cleanup**:
```cpp
checkResourceDestroy();              // Clean up destroyed resources
checkGameObjectDestroy();             // Clean up destroyed objects
checkTriggerDestroy();               // Clean up destroyed triggers
checkCharacterControllerDestroy();   // Clean up destroyed controllers
checkSceneDestroy();                 // Clean up destroyed scenes
```

Uses deferred deletion queues for safe Vulkan cleanup.

### 3. Render (Frame Submission)

```cpp
engine->render();
```

Records and submits Vulkan frame:

**3.1 Frame Synchronization**:
```cpp
waitForFence(inFlightFences[currentFrame]);
resetFence(inFlightFences[currentFrame]);
```

Wait for GPU to finish previous frame before reusing buffers.

**3.2 Acquire Swapchain Image**:
```cpp
vkAcquireNextImageKHR(swapChain, imageAvailableSemaphore, ...)
```

Get next image to render to.

**3.3 Update Uniform Buffers**:
```cpp
updateUniformBuffer(currentFrame, ubo);
// Contains view and projection matrices
```

**3.4 Command Buffer Recording**:
```cpp
beginCommandBuffer(commandBuffer);

vkCmdBeginRenderPass(renderPass);
  vkCmdBindPipeline(graphicsPipeline);
  
  forEach (GameObject with mesh in scene) {
      bindVertexBuffer(mesh->vertices);
      bindIndexBuffer(mesh->indices);
      bindDescriptorSets(frameDescriptorSet, textureDescriptorSet);
      vkCmdDrawIndexed(mesh->indexCount);
  }
  
  renderUI();      // UI elements
  renderImGui();   // Debug UI
vkCmdEndRenderPass();

endCommandBuffer(commandBuffer);
```

**3.5 Command Buffer Submission**:
```cpp
VkSubmitInfo submitInfo = {
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &imageAvailableSemaphore,
    .commandBufferCount = 1,
    .pCommandBuffers = &commandBuffer,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &renderFinishedSemaphore
};
vkQueueSubmit(graphicsQueue, &submitInfo, inFlightFence);
```

Submit recorded commands to GPU.

**3.6 Swapchain Presentation**:
```cpp
VkPresentInfoKHR presentInfo = {
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &renderFinishedSemaphore,
    .swapchainCount = 1,
    .pSwapchains = &swapChain,
    .pImageIndices = &imageIndex
};
vkQueuePresentKHR(presentQueue, &presentInfo);
```

Display rendered frame on screen.

## Frame Timing

### Delta Time

```cpp
float getDeltaTime();
// Returns: seconds elapsed since last frame
```

Used for frame-rate independent movement:

```cpp
void Update(Engine* engine) {
    float dt = engine->getDeltaTime();
    Vector3 moveAmount = moveDirection * speed * dt;
    transform.position += moveAmount;
}
```

### Frame Rate

Engine targets v-sync (60 FPS typically):
- Waits for vertical blank before presenting
- Prevents screen tearing
- Provides consistent timing

### Timing Accuracy

Delta time is accurate to milliseconds; suitable for:
- Physics simulation
- Animation playback
- Smooth movement
- Event scheduling with `addTimer()`

## Update Order Summary

```
while (engine->running()) {
    │
    ├─ Scene::UpdateScene()        [USER LOGIC]
    │
    ├─ Engine::update()
    │  ├─ Input polling (glfwPollEvents)
    │  ├─ Delta time calculation
    │  ├─ GameObject::Update() for each object [USER LOGIC]
    │  ├─ Physics simulation (PhysX step)
    │  ├─ Collision processing
    │  ├─ Transform synchronization
    │  ├─ Camera matrix update
    │  ├─ Audio system update
    │  ├─ Timer processing (callbacks)
    │  └─ Deferred destruction (cleanup queues)
    │
    └─ Engine::render()
       ├─ Swapchain synchronization
       ├─ Uniform buffer update
       ├─ Vulkan command buffer recording
       │  ├─ Render pass begin
       │  ├─ Draw GameObjects
       │  ├─ Draw UI elements
       │  ├─ Render ImGui
       │  └─ Render pass end
       ├─ Command buffer submission
       └─ Swapchain presentation
```

## Scene Transitions

Scenes are transitioned at specific points in the loop:

```cpp
while (engine->running()) {
    engine->updateScene();
    engine->update();
    engine->render();
    
    // Check if scene wants to transition
    if (engine->isLastFrame()) {  // Transitioned from previous frame
        auto scene = static_cast<MyScene*>(engine->getActiveScene());
        if (scene && scene->loadNewScene) {
            scene->loadNewScene = false;
            engine->loadScene(scene->sceneToLoad);  // Load new scene
            // Old scene: DestroyScene() called, objects destroyed
            // New scene: EarlyInitScene(), then InitScene() called
        }
    }
}
```

**Timing**:
- Scene load request happens in `updateScene()` or `update()`
- Actual transition happens on next loop iteration after `render()`
- Ensures all systems complete before switching scenes

## Initialization Sequence

Called once when engine starts:

```cpp
engine->init(width, height, "title");
```

**Sequence**:

1. **Window Creation** - GLFW window initialization
2. **Vulkan Initialization**:
   - Instance creation
   - Physical device selection
   - Logical device and queues
   - Swapchain and framebuffers
   - Renderpass and graphics pipeline
   - Command pools and buffers
   - Descriptor pools and layouts
   - Semaphores and fences
3. **PhysX Initialization**:
   - Physics foundation
   - Physics scene
   - Default material
4. **FMOD Audio Initialization**:
   - FMOD system creation
   - Channel groups
   - 3D listener setup
5. **ImGui Setup**:
   - ImGui context creation
   - GLFW and Vulkan backends
6. **Input System**:
   - GLFW input callbacks
   - Gamepad polling setup
7. **DualSense Setup** (if available):
   - Controller detection
   - Haptics support

## Cleanup Sequence

Called when engine shuts down:

```cpp
engine->cleanup();
Engine::Destroy(engine);
```

**Sequence**:

1. **Active Scene Cleanup** - `DestroyScene()` called
2. **GameObjects Destroyed** - All objects in scene destroyed
3. **Resources Destroyed** - Meshes, textures, sounds freed
4. **ImGui Cleanup** - ImGui context destroyed
5. **FMOD Cleanup** - Audio system shut down
6. **PhysX Cleanup** - Physics scene and foundation cleaned up
7. **Vulkan Cleanup**:
   - Wait for device idle
   - Destroy pipelines, shaders, descriptors
   - Destroy buffers and images
   - Destroy swapchain and framebuffers
   - Destroy device and instance
8. **Window Cleanup** - GLFW window destroyed

## Best Practices

1. **Keep UpdateScene fast**: Complex logic should be in GameObject::Update()
2. **Don't create/destroy in Update**: Use request methods, let engine cleanup
3. **Frame-rate independent**: Always use `getDeltaTime()` for movement
4. **Input in UpdateScene**: Process input for global logic, per-object in GameObject::Update()
5. **Physics continuous**: Don't manually move objects with large jumps; use forces instead
6. **Timer precision**: Timers are accurate to about 1 frame; use for event scheduling, not animation
7. **Avoid blocking calls**: Don't use sleep() or wait(); will freeze the game
8. **Profile bottlenecks**: Use engine profiling to find slow systems

## Checking Loop Status

```cpp
bool running = engine->running();
// false when window closed or engine->exit() called

bool lastFrame = engine->isLastFrame();
// true for one frame after render, useful for deferred operations
```

## Exit Handling

Request engine shutdown gracefully:

```cpp
engine->exit();
// Sets internal flag; loop exits on next iteration
```

The main loop then:
```cpp
while (engine->running()) {  // Now false
    // Loop exits
}

engine->cleanup();
```
