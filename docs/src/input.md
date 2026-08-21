# Input

## Input System Overview

VkEngine polls input devices every frame and provides immediate query APIs. Supported devices include keyboard, mouse, gamepad, and DualSense controllers (PlayStation/Windows). Input is platform-independent; use abstract key codes and button enums.

## Keyboard Input

### Querying Key State

```cpp
KeyState getKey(KeyCode code);
// Returns: PRESS or RELEASE (current frame state)
```

**Usage**:
```cpp
if (engine->getKey(KeyCode::W) == PRESS) {
    playerMoveForward();
}

if (engine->getKey(KeyCode::Space) == PRESS && isGrounded) {
    playerJump();
}
```

### Key Codes

VkEngine provides GLFW key code mappings:

**Letters**: `KeyCode::A` through `KeyCode::Z`

**Numbers**: `KeyCode::Key0` through `KeyCode::Key9`

**Function Keys**: `KeyCode::F1` through `KeyCode::F25`

**Special Keys**:
```cpp
KeyCode::Escape
KeyCode::Enter
KeyCode::Tab
KeyCode::Backspace
KeyCode::Delete
KeyCode::Insert
KeyCode::Home
KeyCode::End
KeyCode::PageUp
KeyCode::PageDown
KeyCode::Up
KeyCode::Down
KeyCode::Left
KeyCode::Right
```

**Modifiers**:
```cpp
KeyCode::LeftShift
KeyCode::RightShift
KeyCode::LeftControl
KeyCode::RightControl
KeyCode::LeftAlt
KeyCode::RightAlt
KeyCode::LeftSuper
KeyCode::RightSuper
```

**Keypad**:
```cpp
KeyCode::KP0 through KeyCode::KP9
KeyCode::KPDecimal
KeyCode::KPDivide
KeyCode::KPMultiply
KeyCode::KPSubtract
KeyCode::KPAdd
KeyCode::KPEnter
KeyCode::KPEqual
```

## Mouse Input

### Mouse Position

```cpp
Vector2 getMousePos();
// Returns: cursor position in screen space (pixels from top-left)
```

### Mouse Buttons

```cpp
KeyState getMouseButton(MouseButton button);
// Returns: PRESS or RELEASE
```

**MouseButton Enum**:
```cpp
MouseButton::Left      // Primary button
MouseButton::Right     // Secondary button
MouseButton::Middle    // Scroll button
MouseButton::Button4 through Button8  // Extra buttons
```

### Mouse Scroll

```cpp
float getScrollDelta();
// Returns: scroll wheel movement this frame
// Positive = scroll up, negative = scroll down
```

### Mouse Ray (3D Picking)

Convert screen coordinates to 3D ray for raycasting:

```cpp
Vector3 rayOrigin;
Vector3 rayDirection;
engine->getMouseRay(rayOrigin, rayDirection);

// Now raycast
RaycastHit hit = engine->raycast(rayOrigin, rayDirection, 100.0f);
if (hit.object) {
    onObjectClicked(hit.object);
}
```

The ray is calculated from inverse view-projection matrix.

## Cursor Control

### Cursor Modes

```cpp
void setCursorMode(CursorMode mode);

enum CursorMode {
    NORMAL,      // Visible, unrestricted (default)
    HIDDEN,      // Hidden but functional
    DISABLED,    // Captured by window, invisible
    CAPTURED     // Platform-dependent capture mode
};
```

**Example**: First-person camera setup

```cpp
engine->setCursorMode(CursorMode::DISABLED);  // Capture cursor

// In update loop
Vector2 mousePos = engine->getMousePos();
// Calculate camera rotation from mouse movement
```

## Gamepad Input

### Gamepad State

```cpp
GamepadState* getGamepad();
// Returns: current gamepad state (may be null if no gamepad connected)
```

### GamepadState Structure

```cpp
struct GamepadState {
    unsigned char buttons[15];   // Button states: GLFW_PRESS or GLFW_RELEASE
    float axes[6];               // Axis values: -1.0 to 1.0
};
```

### Button Indices

```cpp
GAMEPAD_BUTTON_A                // Face button south
GAMEPAD_BUTTON_B                // Face button east
GAMEPAD_BUTTON_X                // Face button west
GAMEPAD_BUTTON_Y                // Face button north

GAMEPAD_BUTTON_LEFT_BUMPER      // LB / L1
GAMEPAD_BUTTON_RIGHT_BUMPER     // RB / R1

GAMEPAD_BUTTON_BACK             // Select / Back
GAMEPAD_BUTTON_START            // Start
GAMEPAD_BUTTON_GUIDE            // Xbox button / PS button

GAMEPAD_BUTTON_LEFT_THUMB       // Left stick click
GAMEPAD_BUTTON_RIGHT_THUMB      // Right stick click

GAMEPAD_BUTTON_DPAD_UP
GAMEPAD_BUTTON_DPAD_RIGHT
GAMEPAD_BUTTON_DPAD_DOWN
GAMEPAD_BUTTON_DPAD_LEFT
```

### Axis Indices

```cpp
GAMEPAD_AXIS_LEFT_X             // Left stick horizontal
GAMEPAD_AXIS_LEFT_Y             // Left stick vertical

GAMEPAD_AXIS_RIGHT_X            // Right stick horizontal
GAMEPAD_AXIS_RIGHT_Y            // Right stick vertical

GAMEPAD_AXIS_LEFT_TRIGGER       // LT / L2 (0 to 1)
GAMEPAD_AXIS_RIGHT_TRIGGER      // RT / R2 (0 to 1)
```

### Gamepad Usage Example

```cpp
GamepadState* pad = engine->getGamepad();
if (pad) {
    // Movement
    float moveX = pad->axes[GAMEPAD_AXIS_LEFT_X];
    float moveY = pad->axes[GAMEPAD_AXIS_LEFT_Y];
    Vector3 moveDir = {moveX, 0, moveY};
    player->Move(moveDir, 10.0f, engine->getDeltaTime());
    
    // Camera
    float camX = pad->axes[GAMEPAD_AXIS_RIGHT_X];
    float camY = pad->axes[GAMEPAD_AXIS_RIGHT_Y];
    engine->cameraRotation.y += camX * 2.0f;  // Yaw
    engine->cameraRotation.x += camY * 2.0f;  // Pitch
    
    // Actions
    if (pad->buttons[GAMEPAD_BUTTON_A] == GLFW_PRESS) {
        playerJump();
    }
    if (pad->buttons[GAMEPAD_BUTTON_X] == GLFW_PRESS) {
        playerInteract();
    }
}
```

## DualSense Controller (PlayStation)

### Detection

```cpp
bool isDualSenseAttached();
// Returns: true if DualSense is connected
```

### Haptics

Play haptic feedback using sound data:

```cpp
void dualsense_playHaptics(Sound* sound, float volume);
// volume: 0.0 to 1.0
```

Only works on Windows 11+ with DualSense connected.

### Lightbar Control

Set DualSense controller lightbar color:

```cpp
void dualsense_setLightbarColor(unsigned char R, unsigned char G, unsigned char B);
// RGB values: 0-255
```

**Example**: Color-coded status indicator

```cpp
if (playerHealth > 50) {
    engine->dualsense_setLightbarColor(0, 255, 0);    // Green (healthy)
} else if (playerHealth > 25) {
    engine->dualsense_setLightbarColor(255, 165, 0);  // Orange (injured)
} else {
    engine->dualsense_setLightbarColor(255, 0, 0);    // Red (critical)
}
```

## Input Processing Pattern

### Update Loop Pattern

```cpp
void UpdateScene(Engine* engine) {
    // Poll input
    Vector3 moveDir = {};
    if (engine->getKey(KeyCode::W) == PRESS) moveDir.z += 1;
    if (engine->getKey(KeyCode::S) == PRESS) moveDir.z -= 1;
    if (engine->getKey(KeyCode::A) == PRESS) moveDir.x -= 1;
    if (engine->getKey(KeyCode::D) == PRESS) moveDir.x += 1;
    
    // Gamepad alternatives
    GamepadState* pad = engine->getGamepad();
    if (pad) {
        moveDir.x = pad->axes[GAMEPAD_AXIS_LEFT_X];
        moveDir.z = pad->axes[GAMEPAD_AXIS_LEFT_Y];
    }
    
    // Apply movement
    player->Move(moveDir, 10.0f, engine->getDeltaTime());
    
    // Jump
    if ((engine->getKey(KeyCode::Space) == PRESS ||
         (pad && pad->buttons[GAMEPAD_BUTTON_A] == GLFW_PRESS)) 
        && isGrounded) {
        player->Jump(15.0f);
    }
    
    // Interact
    if (engine->getKey(KeyCode::E) == PRESS) {
        handleInteraction();
    }
}
```

### Input Filtering

```cpp
// Prevent repeated actions from held keys
bool jumpPressed = false;

void Update(Engine* engine) {
    bool jumpKeyDown = (engine->getKey(KeyCode::Space) == PRESS);
    
    if (jumpKeyDown && !jumpPressed && isGrounded) {
        player->Jump(15.0f);
        jumpPressed = true;
    }
    
    if (!jumpKeyDown) {
        jumpPressed = false;
    }
}
```

### Mouse Interaction Example

```cpp
void MyScene::UpdateScene(Engine* engine) {
    // Check for click
    if (engine->getMouseButton(MouseButton::Left) == PRESS && !clickProcessed) {
        // Get 3D ray from mouse
        Vector3 rayOrigin, rayDirection;
        engine->getMouseRay(rayOrigin, rayDirection);
        
        // Raycast
        RaycastHit hit = engine->raycast(rayOrigin, rayDirection, 100.0f);
        if (hit.object && hit.object->tag == "interactive") {
            hit.object->onInteract();
            clickProcessed = true;
        }
    }
    
    if (engine->getMouseButton(MouseButton::Left) == RELEASE) {
        clickProcessed = false;
    }
}
```

## Input Implementation Details

### Polling Frequency

- Input polled every frame via `glfwPollEvents()`
- State available immediately after polling
- No input buffering (only current frame state)

### Coordinate System

- **Screen Space**: (0, 0) at top-left, X right, Y down
- **World Space**: Used for raycast conversion

### Frame Timing

- All input queries return state for current frame
- Held keys return PRESS every frame (not RELEASE/REPEAT)
- Use state flags to detect transitions (held vs just-pressed)

## Tips and Best Practices

1. **Use abstractions**: Create input manager class to map keys to actions
2. **Support both input methods**: Allow keyboard and gamepad for same action
3. **Handle missing gamepads gracefully**: Check `getGamepad()` before use
4. **Debounce input**: Track state changes, not raw pressed states
5. **First-person movement**: Use relative mouse mode (DISABLED) for smooth camera
6. **Menu navigation**: Use keyboard for menus on desktop, gamepad on console
7. **Platform differences**: Test on all target platforms (input APIs may vary)
