#include "engine.h"

void Engine::init(const int width, const int height, const char* title) {
	printf("%i\n", sizeof(GameObject));
	printf("%i\n", sizeof(Scene));
	
	initWindow(width, height, title);
	initVulkan();

	mHitCallback = new ControllerHitCallback();
	mPhysicsEventListener = new PhysicsEventListener();

	initPhysX();

	initFMOD();

	FMOD::ChannelGroup* master;
	system->getMasterChannelGroup(&master);
	master->setMute(true);

	// create default material
	PhysicsMaterial* mat = createPhysicsMaterial(0.5f, 0.4f, 0.2f);
	gDefaultMaterial = mat->material;
	eDefaultMaterial = mat;

	// initialize imgui ui
	InitImGui(window, instance, physicalDevice, device, graphicsQueue, renderPass, MAX_FRAMES_IN_FLIGHT);

	initUILayer();

	// init haptics
	#ifdef _WIN32
	initHaptics();
	initDSRGB();
	#endif

	lightSettings.lightPos = glm::vec3(0.5f, 0.5f, 1.0f);
	lightSettings.ambient = 0.25f;
	lightSettings.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
}

void Engine::initVulkan() {
	createInstance();
	setupDebugMessenger();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
	createImageViews();
	createRenderPass();

	createTextureSampler();  
	createDescriptorPool();
	createDescriptorSetLayouts();
	createUniformBuffers();  
	createFrameDescriptorSets(); 

	createGraphicsPipeline();

	createCommandPool();
	createDepthResources();
	createFramebuffers();
	createCommandBuffer();
	createSyncObjects();
}

bool Engine::running() {
	return !glfwWindowShouldClose(window) && !exitFlag;
}

void Engine::update() {
	glfwPollEvents();
	readGlfwGamePadState();

	float currentTime = glfwGetTime();
	deltaTime = currentTime - oldTime;
	oldTime = currentTime;

	for (auto& object: gameObjects) {
		object->Update();
		object->updateSound();
	}

	executionTime += deltaTime;
	oneShotTimers.update(executionTime);

	updatePhysics(deltaTime);
	UpdateCamera();

	syncListenerPos();

	system->update();

	if (framesToUnmute > 0) framesToUnmute--;
	else {
		if (masterMuted) {
			masterMuted = false;

			FMOD::ChannelGroup* master;
			system->getMasterChannelGroup(&master);
			master->setMute(globalMute);
		}
	}

	bool isLastBufferSlot = (currentFrame == MAX_FRAMES_IN_FLIGHT - 1);
	if (isLastBufferSlot) {
		// last frame was rendered, check stuff to destroy before starting new cycle
		checkDestroy();
	}
}

void Engine::render() {
	drawFrame();
}

void Engine::exit() {
	exitFlag = true;
}

float Engine::getDeltaTime() {
	return deltaTime;
}

void Engine::cleanup() {
	vkDeviceWaitIdle(device);

	unloadActiveScene();

	//cleanupUILayer();

	ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(device, imguiPool, nullptr);

	for (auto& obj : gameObjects) {
		cleanupGameObject(obj);
	}

	#ifdef _WIN32
	// Cleanup WASAPI
	if (hAudioEvent) {
		SetEvent(hAudioEvent);
	}

	if (mAudioThread.joinable()) {
		mAudioThread.join(); // Wait for the thread to actually finish
	}

	if (dsPresent) {
		CloseHandle(dsHID);
	}
	#endif

	// FMOD Cleanup
	for (auto& sound : sounds) {
		sound->destroy(this);
	}
	system->close();
	system->release();

	// PhysX cleanup
	for (auto& material : materials) {
		material.material->release();
	}

	for (auto& mesh : meshes) {
		mesh->destroy(this);
	}

	for (auto& playerController : charControllers) {
		playerController->playerController->release();
	}

	if (gControllerManager) {
		gControllerManager->release();
		gControllerManager = nullptr;
	}
	
	gScene->release();
	gPhysics->release();
	gFoundation->release();

	// Vulkan and the rest of cleanup
	for (size_t i = 0; i < imageAvailableSemaphores.size(); ++i) {
		vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
	}
	for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
		vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
	}
	for (size_t i = 0; i < inFlightFences.size(); ++i) {
		vkDestroyFence(device, inFlightFences[i], nullptr);
	}

	vkFreeCommandBuffers(
		device,
		commandPool,
		static_cast<uint32_t>(commandBuffers.size()),
		commandBuffers.data()
	);

	cleanupSwapChain();

	vkDestroySampler(device, textureSampler, nullptr);

	// destroy uniform buffers (one per frame)
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkUnmapMemory(device, uniformBuffersMemory[i]);
		vkDestroyBuffer(device, uniformBuffers[i], nullptr);
		vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(device, descriptorPool, nullptr);	
	
	for (auto& texture : textures) {
		texture->destroy(this);
	}

	vkDestroyDescriptorSetLayout(device, frameSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, textureSetLayout, nullptr);

	vkDestroyCommandPool(device, commandPool, nullptr);

	vkDestroyPipeline(device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);

	if (enableValidationLayers) {
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}

	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);

	// glfw cleanup
	glfwDestroyWindow(window);

	glfwTerminate();
}

Engine* Engine::Create() {
	return new Engine();
}

void Engine::Destroy(Engine* instance) {
	delete instance;
}
 
Mesh* Engine::createMesh(std::string name, const char* path) {
	if (resources.contains(name))
    {
        throw std::runtime_error("Resource name already used: " + name);
    }

	Model* model;

	try {
		model = new Model(path);
	}
	catch (std::exception ex) {
		char errorBuffer[1024];
		snprintf(errorBuffer, 1024, "Failed to load mesh %s", path);

		OnError_Handler(errorBuffer);
		return nullptr;
	}

	Mesh* mesh = new Mesh();
	*mesh = std::move(model->meshes[0]);
	meshes.push_back(mesh);

	createVertexBuffer(mesh->verticesVk, mesh->verticesVkMem, mesh->vertices);
	createIndexBuffer(mesh->indicesVk, mesh->indicesVkMem, mesh->indices);

	cookMesh(mesh);

	resources[name] = mesh;
	mesh->name = name;

	return mesh;
}

Mesh* Engine::createMesh(std::string name, std::vector<Vertex> vertices, std::vector<uint32_t> indices) {
	if (resources.contains(name))
    {
        throw std::runtime_error("Resource name already used: " + name);
    }

	Mesh* mesh = new Mesh(vertices, indices);
	meshes.push_back(mesh);

	createVertexBuffer(mesh->verticesVk, mesh->verticesVkMem, mesh->vertices);
	createIndexBuffer(mesh->indicesVk, mesh->indicesVkMem, mesh->indices);

	cookMesh(mesh);

	resources[name] = mesh;
	mesh->name = name;

	return mesh;
}

void Engine::getCameraVectors(Vector3& forward, Vector3& right) {
	UpdateCamera();

	forward = { camFront.x, camFront.y, camFront.z };
	right = { camRight.x, camRight.y, camRight.z };
}

void Engine::checkDestroy() {
	checkResourceDestroy();
	checkGameObjectDestroy();
	checkTriggerDestroy();
	checkCharacterControllerDestroy();
	checkSceneDestroy();
}

VkDevice* Engine::getVkDevicePtr() {
	return &device;
}

void Engine::setClearColor(Vector3 clearColor) {
	vecClearColor = clearColor;
}

Vector2 Engine::getExtents() {
	return { (float)swapChainExtent.width, (float)swapChainExtent.height };
}

void Engine::setLightPosition(Vector3 pos) {
	lightSettings.lightPos = glm::vec3(pos.x, pos.y, pos.z);
}

bool Engine::isLastFrame() {
	return (currentFrame == MAX_FRAMES_IN_FLIGHT - 1);
}

void Engine::OnError_Handler(std::string errorString) {
	// TODO: Add custom user handler?
	char errorBuffer[1024];
	snprintf(errorBuffer, 1024, "A fatal engine error has occured! The application will be terminated.\n%s", errorString.c_str());

#ifdef _WIN32
	MessageBoxA(NULL, errorBuffer, "Fatal engine error", MB_ICONERROR | MB_OK);
#else
	printf(errorBuffer);
#endif

	#ifdef _WIN32
	DebugBreak();
	#else 
	__builtin_trap();
	#endif

	exit();
}