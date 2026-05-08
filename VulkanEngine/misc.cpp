#include "engine.h"

VkVertexInputBindingDescription Vertex::getBindingDescription() {
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::getAttributeDescriptions() {
	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(Vertex, pos);

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, color);

	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

	return attributeDescriptions;
}

void Engine::populateObjectBuffer(ObjectBuffer& buffer, std::vector<Vertex> vertices, std::vector<uint32_t> indices) {
	createVertexBuffer(buffer.vertexBuffer, buffer.vertexBufferMemory, vertices);
	createIndexBuffer(buffer.indexBuffer, buffer.indexBufferMemory, indices);
}

VkCommandBuffer Engine::beginSingleTimeCommands() {
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void Engine::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);

	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void Engine::createSyncObjects() {
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	// renderFinishedSemaphores: one per swapchain image to avoid reuse while still owned by present
	renderFinishedSemaphores.resize(swapChainImages.size());
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	// create image-available semaphores and per-frame fences
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create synchronization objects for a frame!");
		}
	}

	// create one render-finished semaphore per swapchain image (prevent semaphore reuse)
	for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create renderFinished semaphore!");
		}
	}
}

void Engine::updateUniformBuffer(uint32_t currentFrame) {
	char* dataPtr = static_cast<char*>(uniformBuffersMapped[currentFrame]);

	memset(dataPtr, 0, uboSize_T);

	int i = 0;

	for (auto& object: gameObjects) {
		if (!object) { i++; continue; }

		size_t offset = i * dynamicAlignment;

		object->uboData.view = getViewMatrix();
		auto proj = getProjectionMatrix();
		object->uboData.proj = proj;
		object->uboData.model = object->GetModel();

		memcpy(dataPtr + offset, &object->uboData, sizeof(UniformBufferObject));

		i++;
	}

	for (auto& element : uiElements) {
		if (!element) continue;

		std::cout << "UI element slot in UBO: " << i << std::endl;

		size_t offset = i * dynamicAlignment;

		glm::mat4 model = glm::mat4(1.0f);

		Vector3 pos = { element->position.x, element->position.y, 1.0f };
		Vector3 size = { element->size.x, element->size.y, 1.0f };
		model = glm::translate(model, Vec3toGlm(pos));
		model = glm::scale(model, Vec3toGlm(size));

		element->ubo.model = model;
		element->ubo.view = glm::mat4(1.0f);

		glm::mat4 proj = glm::ortho(
			0.0f, (float)swapChainExtent.width,
			(float)swapChainExtent.height, 0.0f,
			-1.0f, 1.0f
		);

		element->ubo.proj = proj;

		memcpy(dataPtr + offset, &element->ubo, sizeof(UniformBufferObject));

		i++;
	}
}

std::vector<VRAMStats> Engine::getVRAMStats() {
    std::vector<VRAMStats> stats;

    VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetProps = {};
    budgetProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;

    VkPhysicalDeviceMemoryProperties2 memProps = {};
    memProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    memProps.pNext = &budgetProps;

    vkGetPhysicalDeviceMemoryProperties2(physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryProperties.memoryHeapCount; i++) {
        // Only grab heaps that are actually on the GPU (DEVICE_LOCAL)
        if (memProps.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            VRAMStats s;
            s.heapIndex = i;
            s.usageMB = (float)budgetProps.heapUsage[i] / (1024.0f * 1024.0f);
            s.budgetMB = (float)budgetProps.heapBudget[i] / (1024.0f * 1024.0f);
            stats.push_back(s);
        }
    }

    return stats;
}

std::string IResource::getName() {
	return name;
}

void Engine::pushRayDebug(RayDebug rd) {
	gRayDebugs.push_back(rd);
}