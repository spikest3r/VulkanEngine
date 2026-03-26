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

	// init haptics
	initHaptics();
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

	createDescriptorSetLayout();
	createTextureSampler();  
	createUniformBuffers();  
	createDescriptorPool();  

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
	readDualSenseState();

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

	ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(device, imguiPool, nullptr);

	for (auto& obj : gameObjects) {
		cleanupGameObject(obj);
	}

	// Cleanup WASAPI
	if (hAudioEvent) {
		SetEvent(hAudioEvent);
	}

	if (mAudioThread.joinable()) {
		mAudioThread.join(); // Wait for the thread to actually finish
	}

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

	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

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

void Engine::cookMesh(Mesh* mesh) {

#ifdef _WIN32
	PxCookingParams params(gPhysics->getTolerancesScale());

	std::cout << "Vertex count: " << mesh->vertices.size() << std::endl;
	std::cout << "Index count: " << mesh->indices.size() << std::endl;

	for (int i = 0; i < 5; i++) {
		auto& v = mesh->vertices[i];
		std::cout << "v" << i << ": " << v.pos.x << " " << v.pos.y << " " << v.pos.z << std::endl;
	}

	{
		PxTriangleMeshDesc meshDesc;
		std::vector<PxVec3> pxVerts;
		pxVerts.reserve(mesh->vertices.size());
		for (auto& v : mesh->vertices) {
			pxVerts.push_back(PxVec3(v.pos.x, v.pos.y, v.pos.z));
		}

		// Then use pxVerts for cooking:
		meshDesc.points.count = (PxU32)pxVerts.size();
		meshDesc.points.stride = sizeof(PxVec3);
		meshDesc.points.data = pxVerts.data();

		meshDesc.triangles.count = (PxU32)mesh->indices.size() / 3;
		meshDesc.triangles.stride = sizeof(uint32_t) * 3;
		meshDesc.triangles.data = mesh->indices.data();

		PxDefaultMemoryOutputStream writeBuffer;

		if (!PxCookTriangleMesh(params, meshDesc, writeBuffer))
			return;

		PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
		mesh->triMesh = gPhysics->createTriangleMesh(readBuffer);
	}

	{
		PxConvexMeshDesc convexDesc;
		convexDesc.points.count = (PxU32)mesh->vertices.size();
		convexDesc.points.stride = sizeof(Vertex);
		convexDesc.points.data = mesh->vertices.data();
		convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

		PxDefaultMemoryOutputStream writeBuffer;

		if (!PxCookConvexMesh(params, convexDesc, writeBuffer))
			return;

		PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
		mesh->convexMesh = gPhysics->createConvexMesh(readBuffer);
	}

#else
	{
		PxTriangleMeshDesc meshDesc;
		meshDesc.points.count = (PxU32)mesh->vertices.size();
		meshDesc.points.stride = sizeof(Vertex);
		meshDesc.points.data = mesh->vertices.data();

		meshDesc.triangles.count = (PxU32)mesh->indices.size() / 3;
		meshDesc.triangles.stride = sizeof(uint32_t) * 3;
		meshDesc.triangles.data = mesh->indices.data();

		PxDefaultMemoryOutputStream writeBuffer;

		if (!mCooking->cookTriangleMesh(meshDesc, writeBuffer))
			return;

		PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
		mesh->triMesh = gPhysics->createTriangleMesh(readBuffer);
	}

	{
		PxConvexMeshDesc convexDesc;
		convexDesc.points.count = (PxU32)mesh->vertices.size();
		convexDesc.points.stride = sizeof(Vertex);
		convexDesc.points.data = mesh->vertices.data();
		convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

		PxDefaultMemoryOutputStream writeBuffer;

		if (!mCooking->cookConvexMesh(convexDesc, writeBuffer))
			return;

		PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
		mesh->convexMesh = gPhysics->createConvexMesh(readBuffer);
	}

#endif
}
 
Mesh* Engine::createMesh(std::string name, const char* path) {
	if (resources.contains(name))
    {
        throw std::runtime_error("Resource name already used: " + name);
    }

	Model model = Model(path);
	Mesh* mesh = new Mesh();
	*mesh = std::move(model.meshes[0]);
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