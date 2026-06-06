#pragma once
#include <vulkan/vulkan.h>
#include "texture.h"
#include "engine_types.h"

class Engine;

class UIElement {
	friend class Engine;
public:
	Vector2 position;
	Vector2 size;
private:
	Texture* texture;
	ObjectBuffer buffer;
	UniformBufferObject ubo;
};