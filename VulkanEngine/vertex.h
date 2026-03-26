#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <glm/glm.hpp>

struct Vertex {
	glm::vec4 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription getBindingDescription();

	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};