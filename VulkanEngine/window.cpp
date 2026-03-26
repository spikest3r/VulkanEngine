#include "engine.h"

void Engine::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	mouseX = xpos;
	mouseY = ypos;
}

Vector2 Engine::getMousePos() {
	return { mouseX, mouseY };
}

KeyState Engine::getKey(KeyCode code) {
	if (glfwGetKey(window, (int)code) == GLFW_PRESS) { 
		return PRESS;
	}
	else return RELEASE;
}

KeyState Engine::getMouseButton(MouseButton button)
{
    int state = glfwGetMouseButton(window, static_cast<int>(button));

    switch (state)
    {
        case GLFW_PRESS:   return PRESS;
        case GLFW_RELEASE: return RELEASE;
        default:            return RELEASE;
    }
}

void Engine::initWindow(const int width, const int height, const char* title) {
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width, height, title, nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

	glfwSetCursorPosCallback(window, mouseCallback);
	// glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Engine::setCursorMode(CursorMode mode) {
    ImGuiIO& io = ImGui::GetIO();

    switch (mode) {
    case NORMAL:
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse; // UI can use mouse
        break;
    case HIDDEN:
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        break;
    case DISABLED:
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse; 
        break;
    case CAPTURED:
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_CAPTURED);
        break;
    }

	if (mode == DISABLED) {
		glfwSetCursor(window, nullptr); // Explicitly clear the cursor object
	}
}

void Engine::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
	app->framebufferResized = true;
}

void Engine::mouseCallback(GLFWwindow* window, double x, double y) {
	auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
	app->mouse_callback(window, x, y);
}