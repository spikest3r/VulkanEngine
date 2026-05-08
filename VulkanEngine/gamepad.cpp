#include "engine.h"

void Engine::readGlfwGamePadState() {
    GLFWgamepadstate state;

    if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
        for (int i = 0; i < 6; i++) {
            gamepadState.axes[i] = state.axes[i];
        }
        for (int i = 0; i < 15; i++) {
            gamepadState.buttons[i] = state.buttons[i];
        }
    }
}

GamepadState* Engine::getGamepad() {
    return &gamepadState;
}