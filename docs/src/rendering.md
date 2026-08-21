# Rendering

## Vulkan-Based Rendering Pipeline

VkEngine uses Vulkan for high-performance 3D graphics with forward rendering. The engine handles all Vulkan setup and management—developers work with high-level GameObject and Scene APIs.

## Rendering Architecture

### Core Components

**Initialization** (in `Engine::init()`):
- Vulkan instance with required extensions
- Physical device selection
- Logical device with graphics and present queues
- Swapchain for window output
- Render pass defining attachment formats and layout
- Graphics pipeline with vertex and fragment shaders
- Descriptor pool for resource binding
- Framebuffers for each swapchain image

**Frame Synchronization**:
- Double buffering with `MAX_FRAMES_IN_FLIGHT = 2`
- Per-frame uniform buffers for camera matrices
- Synchronization primitives (fences, semaphores)
- Command buffers recorded and submitted per frame

### Rendering Loop

```cpp
Engine::render() // Called once per frame
  1. Wait for previous frame fence
  2. Acquire next swapchain image
  3. Update uniform buffer with current view/projection matrices
  4. Begin command buffer recording
     - Start render pass
     - Bind graphics pipeline
     - For each GameObject with a mesh:
       - Bind mesh vertex/index buffers
       - Update model matrix UBO
       - Draw indexed vertices
     - Render UI elements
     - Render ImGui
     - End render pass
  5. Submit command buffer to graphics queue
  6. Present swapchain image to screen
```

## Camera System

The engine provides a simple camera system with configurable properties:

```cpp
Vector3 cameraPosition;    // World position (default: 0, 0, 5)
Vector3 cameraRotation;    // Euler angles in degrees (default: 0, -90, 0)
Vector3 cameraOffset;      // Offset from target position (default: 0, 0, 0)
NearFarPlanes planes;      // Near/far clipping planes (default: 0.1, 100)

// Example: follow object with offset
void updateCamera(Engine* engine, GameObject* target) {
    engine->cameraPosition = target->transform.position + Vector3(0, 2, -5);
    engine->cameraRotation = {0, -90, 0};  // Look forward
}
```

**Projection**:
- Field of view: 45°
- Aspect ratio: window width / height
- Orthogonal near/far clipping planes

**View Matrix**: Calculated from `cameraPosition` and `cameraRotation`

## Shader System

### Shaders

Default shaders are compiled to SPIR-V bytecode:
- `vert.spv` - Vertex shader
- `frag.spv` - Fragment shader

Located in engine shader directory.

### Vertex Input

```cpp
struct Vertex {
    glm::vec4 pos;      // Position + padding
    glm::vec3 color;    // Vertex color
    glm::vec2 texCoord; // Texture coordinates
};
```

### Uniform Buffers

Updated per-frame and per-object:

```cpp
struct UniformBufferObject {
    glm::mat4 model;  // Object-to-world transformation
    glm::mat4 view;   // World-to-camera transformation
    glm::mat4 proj;   // Camera-to-normalized device coordinates
};

struct LightPushConstants {
    glm::vec3 lightPos;     // Directional light direction
    float ambient;          // Ambient light multiplier
    glm::vec3 lightColor;   // Light color (RGB)
    uint32_t unlit;         // 1 = unlit, 0 = lit with light
};
```

## Materials and Textures

### Texture Binding

Each texture has a descriptor set for shader binding:

```cpp
// In fragment shader
layout(set=1, binding=0) uniform sampler2D texSampler;
```

**Texture Sampler**:
- Linear filtering for smooth sampling
- Clamp to edge wrapping
- Supports anisotropic filtering (hardware-dependent)

### Updating Textures at Runtime

```cpp
void Update(Engine* engine) {
    if (takeDamage) {
        Texture* damagedTex = engine->getTexture("rock_damaged");
        updateTexture(damagedTex);  // Change surface appearance
    }
}
```

## Drawing GameObjects

### Per-Frame Pipeline

For each GameObject with a mesh:

1. **Model Matrix**: `GetModel()` transforms object from local space to world space
   - Calculated from position, rotation (quaternion), and scale
   - Automatically updated when `transform` changes

2. **Binding**: Mesh vertex/index buffers bound to command buffer

3. **Draw Call**: Indexed draw with vertex count from mesh

4. **Descriptor Sets**: 
   - Frame descriptor set (UBO for camera matrices)
   - Texture descriptor set (sampled in fragment shader)

### Optimization

- Single render pass per frame
- Minimal state changes (objects with same texture bound together is implicit)
- No explicit frustum culling (all objects rendered)
- Command buffers recorded fresh each frame

## UI System

### ImGui Integration

ImGui is integrated for debug UI and in-game overlays:

```cpp
void SetUICallback(std::function<void(Engine*)> callback);
```

**Usage**:
```cpp
engine->SetUICallback([](Engine* engine) {
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::Begin("Debug");
    ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);
    ImGui::End();
});
```

### UI Elements

Rendered 2D elements for HUD:

```cpp
UIElement* createUIElement(Texture* texture, Vector2 pos, Vector2 size);

struct UIElement {
    Vector2 position;  // Screen position in pixels
    Vector2 size;      // Screen size in pixels
    // (texture managed internally)
};
```

**Example**: Crosshair HUD element
```cpp
void MyScene::InitScene(Engine* engine) {
    Texture* crosshair = engine->getTexture("crosshair");
    ui_crosshair = engine->createUIElement(crosshair, {400, 300}, {32, 32});
}
```

## Debug Rendering

### Physics Debug Visualization

Render PhysX shapes to debug physics:

```cpp
engine->renderPhysXDebug(true);   // Enable
engine->renderPhysXDebug(false);  // Disable
```

Shows wireframe collider shapes and actor positions.

### Raycast Visualization

Debug raycasts with red/green lines:

```cpp
struct RayDebug {
    Vector3 origin;        // Start point
    Vector3 hitOrEnd;      // Hit point or end if no hit
    bool hit;              // Whether raycast hit something
};

RayDebug ray = {rayOrigin, hitPoint, true};
engine->pushRayDebug(ray);
// Rendered as line in next frame
```

## Rendering Configuration

### Clear Color

```cpp
engine->setClearColor(Vector3(0.1f, 0.1f, 0.1f));  // Dark gray
```

### Light Positioning

```cpp
engine->setLightPosition(Vector3(1, 1, -1));  // Directional light direction
```

### Ground Plane

Optional ground plane for level layout visualization:

```cpp
engine->setGroundPlaneActive(true);   // Show
engine->setGroundPlaneActive(false);  // Hide
```

## GPU Memory and VRAM Statistics

```cpp
std::vector<VRAMStats> getVRAMStats();
// Returns GPU memory usage and allocation info
```

## Graphics Pipeline Details

### Vulkan Extensions

**Windows**: VK_KHR_win32_surface

**Linux**: VK_KHR_wayland_surface (or xcb)

### Render Pass

- **Format**: Optimal for platform (typically BGRA8 on Windows, RGBA8 on Linux)
- **Attachment**: Single color attachment
- **Depth**: No depth attachment (2.5D or depth-disabled rendering)
- **Load Op**: Clear to specified color

### Pipeline State

- **Topology**: Triangle list
- **Winding**: Counter-clockwise
- **Culling**: Back-face culling enabled
- **Depth Test**: Disabled (no depth buffer)
- **Blending**: Disabled (opaque rendering)

### Swapchain

- **Mode**: FIFO (vsync) - waits for vertical blank
- **Images**: Double buffered (2 images)
- **Format**: Device-optimal format (UNORM color space)

## Performance Considerations

### Current Bottlenecks

- No frustum culling: all objects rendered regardless of camera view
- No LOD system: no level-of-detail mesh switching
- Single pass rendering: no deferred rendering
- No batch rendering: each object is separate draw call

### Optimization Opportunities

1. **Frustum Culling**: Skip GameObjects outside camera view
2. **Instancing**: Render multiple instances with single draw call
3. **Deferred Rendering**: Render to G-buffer for complex lighting
4. **Texture Atlasing**: Combine textures to reduce state changes
5. **Mesh Optimization**: Reduce vertex count and optimize indices

### Known Limitations

- No compute shaders
- No tessellation shaders
- Single directional light
- No normal mapping or parallax mapping
- No post-processing effects
- Fixed vertex layout (position, color, texcoord)
