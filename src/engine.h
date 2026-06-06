#pragma once
#ifndef _HAS_STD_BYTE
#define _HAS_STD_BYTE 0
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

// ImGui core
#include "imgui.h"

// ImGui backends
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include <vulkan/vulkan.h>

#ifdef _WIN32

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#elif __linux__

#define GLFW_EXPOSE_NATIVE_WAYLAND
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#endif

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <optional>
#include <vector>
// #include <deque>
#include <set>
#include <limits>
#include <algorithm>
#include <fstream>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <chrono>
#include <thread>
#include "3d_loader.h"

#include "engine_types.h"
#include "gameobject.h"
#include "texture.h"
#include "sound.h"
#include "charactercontroller.h"

#include <PxPhysicsAPI.h>
#include <extensions/PxDefaultAllocator.h>
#include <extensions/PxDefaultErrorCallback.h>
#include <extensions/PxDefaultSimulationFilterShader.h>
#include <common/PxTolerancesScale.h>
#include <cooking/PxCooking.h>
#include <cooking/PxTriangleMeshDesc.h>
#include <cooking/PxConvexMeshDesc.h>

#include <fmod.hpp>

#include <queue>
#include <unordered_set>
#include <unordered_map>

#include "timersys.h"

#include <type_traits>

#include "scene.h"
#include "ui.h"

// dualsense related features
#ifdef _WIN32

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#endif

using namespace physx;

class ControllerHitCallback;

class Engine {
public:
	ENGINE_API void init(const int width, const int height, const char* title);

	ENGINE_API void update();
	ENGINE_API void render();
	ENGINE_API bool running();
	ENGINE_API float getDeltaTime();
	ENGINE_API void cleanup();
	ENGINE_API void exit();

	ENGINE_API void getCameraVectors(Vector3& forward, Vector3& right);
	ENGINE_API Vector2 getExtents();

	static ENGINE_API Engine* Create(); // Static factory
	static ENGINE_API void Destroy(Engine* instance);

	// Game Object

	ENGINE_API Texture* createTexture(std::string name, const char* path);
	ENGINE_API Mesh* createMesh(std::string name, const char* path);
	ENGINE_API Mesh* createMesh(std::string name, std::vector<Vertex> vertices, std::vector<uint32_t> indices);
	ENGINE_API Sound* createSound(std::string name, const char* path, bool looping, bool three_dim);

	ENGINE_API Texture* getTexture(std::string name);
	ENGINE_API Mesh* getMesh(std::string name);
	ENGINE_API Sound* getSound(std::string name);
	ENGINE_API Scene* getScene(std::string sceneFile);
	ENGINE_API GameObject* getGameObject(std::string name);

	ENGINE_API KeyState getKey(KeyCode code);
	ENGINE_API KeyState getMouseButton(MouseButton button);

	ENGINE_API Vector2 getMousePos();

	Vector3 cameraPosition = { 0.0f, 0.0f, 5.0f }; // Start 5 units back
	Vector3 cameraRotation = { 0.0f, -90.0f, 0.0f }; // Point toward the scene
	Vector3 cameraOffset = {0.0f,0.0f,0.0f};
	NearFarPlanes planes = { 0.1f, 100.f };
	
	ENGINE_API void renderPhysXDebug(bool state);
	ENGINE_API void pushRayDebug(RayDebug rd);

	ENGINE_API ICharacterController* createCharacterController(float height, float radius, Vector3 position, PhysicsMaterial* material, bool interactWithActors);
	ENGINE_API PhysicsMaterial* createPhysicsMaterial(float staticFriction, float dynamicFriction, float restitution);

	ENGINE_API Trigger* createBoxTrigger(Vector3 pos, Vector3 size);

	ENGINE_API void setGlobalMute(bool mute);

	ENGINE_API void requestDestroy(IResource* resource);
	ENGINE_API void requestDestroyGameObject(GameObject* object);
	ENGINE_API void requestDestroyTrigger(Trigger* trigger);
	ENGINE_API void requestDestroyCharacterController(ICharacterController* ctrl);
	ENGINE_API void requestDestroyScene(Scene* scene);

	ENGINE_API RaycastHit raycast(Vector3 origin, Vector3 direction, float distance);

	ENGINE_API void setClearColor(Vector3 clearColor);

	ENGINE_API void addTimer(float delay, std::function<void()> cb);

	ENGINE_API void setCursorMode(CursorMode mode);

	ENGINE_API void SetUICallback(std::function<void(Engine* engine)> callback);
	ENGINE_API std::vector<VRAMStats> getVRAMStats();

	ENGINE_API void loadScene(Scene* scene);

	ENGINE_API void unloadActiveScene();
	ENGINE_API Scene* getActiveScene();
	ENGINE_API void updateScene();

	ENGINE_API void setLightPosition(Vector3 pos);

	// TODO: Template
	ENGINE_API UIElement* createUIElement(Texture* texture, Vector2 pos, Vector2 size);

	template <typename T>
	T* createGameObject(
		Transform spawnTransform,
		Mesh* mesh,
		Texture* texture,
		PhysicsMaterial* material,
		bool isDynamic
	) {
		static_assert(std::is_base_of<GameObject, T>::value,
			"T must inherit from GameObject");

		size_t totalSize = sizeof(ObjectHeader) + alignof(T) + sizeof(T);

		void* raw = requestMemory(totalSize);

		// header
		auto* header = (ObjectHeader*)raw;
		header->destroy = &Engine::destroyImpl<T>;

		// object memory after header
		void* objMem = (char*)raw + sizeof(ObjectHeader);

		// alignment fix
		size_t space = totalSize - sizeof(ObjectHeader);

		void* alignedObjMem = std::align(
			alignof(T),
			sizeof(T),
			objMem,
			space
		);

		if (!alignedObjMem)
			throw std::bad_alloc();

		// construct object
		T* object = new (alignedObjMem) T();

		internal_createGameObject(
			object,
			spawnTransform,
			mesh,
			texture,
			material,
			isDynamic
		);

		object->Start();

		return object;
	}

	template <typename T>
	T* createScene(const char* sceneFile, bool* valid)
	{
		static_assert(std::is_base_of<Scene, T>::value,
			"T must inherit from Scene");

		size_t totalSize = sizeof(ObjectHeader) + alignof(T) + sizeof(T);

		void* raw = requestMemory(totalSize);

		// header sits at start
		auto* header = (ObjectHeader*)raw;
		header->destroy = &destroyImpl<T>;

		// object memory starts after header
		void* objMem = (char*)raw + sizeof(ObjectHeader);

		// IMPORTANT: std::align needs mutable space variable
		size_t space = totalSize - sizeof(ObjectHeader);

		void* alignedObjMem = std::align(
			alignof(T),
			sizeof(T),
			objMem,
			space
		);

		if (!alignedObjMem)
			throw std::bad_alloc();

		// construct Scene in-place
		T* object = new (alignedObjMem) T();

		bool result = loadScene_internal(object, sceneFile);
		if (valid) *valid = result;

		return object;
	}

	// Memory Allocator
	ENGINE_API static void* requestMemory(size_t size);
	ENGINE_API static void freeMemory(void* ptr);

	ENGINE_API void dualsense_playHaptics(Sound* sound, float volume);
	ENGINE_API void dualsense_setLightbarColor(unsigned char R, unsigned char G, unsigned char B);
	ENGINE_API bool isDualSenseAttached();

	ENGINE_API GamepadState* getGamepad();

	ENGINE_API bool isLastFrame();

	// engine internal
	void playSound(Sound* sound, FMOD::ChannelGroup* group, FMOD::Channel** channel, bool startPaused);
	VkDevice* getVkDevicePtr();
	PhysicsMaterial* getDefaultMaterial() { return eDefaultMaterial; }
	void forceDestroy();
	VkDescriptorPool getDescriptorPool() { return descriptorPool; }
private:
	Engine() {};

	void OnError_Handler(std::string errorString);

	template <typename T>
	static void destroyImpl(void* p) {
		((T*)p)->~T();
		void* base = (char*)p - sizeof(ObjectHeader);
		freeMemory(base);
	}

	ENGINE_API void internal_createGameObject(
		GameObject* ptr,
		Transform spawnTransform,
		Mesh* mesh,
		Texture* texture,
		PhysicsMaterial* material,
		bool isDynamic
	);

	Vector3 vecClearColor = {0.0f,0.0f,0.0f};

	uint32_t gameObjectID = 0;

	TimerSystem oneShotTimers; // one-shot
	float executionTime = 0.0f;

	// destroy queue
	std::queue<IResource*> destroyQueue;
	void checkResourceDestroy();
	std::queue<GameObject*> gameObjectDestroyQueue;
	void checkGameObjectDestroy();
	std::queue<Trigger*> triggerDestroyQueue;
	void checkTriggerDestroy();
	std::queue<ICharacterController*> characterControllerDestroyQueue;
	void checkCharacterControllerDestroy();
	std::queue<Scene*> sceneDestroyQueue;
	void checkSceneDestroy();

	void checkDestroy();

	float oldTime = 0.0f;
	float deltaTime = 0.0f;

	float mouseX, mouseY;
	
	const bool enableValidationLayers = false;

	bool checkValidationLayerSupport();

	std::vector<const char*> getRequiredExtensions();

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	// GLFW Window
	GLFWwindow* window;
	const uint32_t WIDTH = 800;
	const uint32_t HEIGHT = 600;
	void mouse_callback(GLFWwindow* window, double xpos, double ypos);
	void initWindow(const int width, const int height, const char* title);
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
	static void mouseCallback(GLFWwindow* window, double x, double y);

	// Vulkan Variables and Methods
	void initVulkan();
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;
	VkRenderPass renderPass;
	VkDescriptorSetLayout frameSetLayout;    // was: single descriptorSetLayout
	VkDescriptorSetLayout textureSetLayout;  // new, for textures
	std::vector<VkDescriptorSet> frameDescriptorSets; // one per frame, replaces per-object sets
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	VkCommandPool commandPool;
	VkSampler textureSampler;

	LightPushConstants lightSettings;

	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	std::vector<VkFence> imagesInFlight;

	bool framebufferResized = false;
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;
	VkDescriptorPool descriptorPool;
	size_t uboSize_T;

	// Camera
	glm::vec3 camPos = { 0.f, 1.f, 3.f };
	glm::vec3 camFront;
	glm::vec3 camUp = { 0.f, 0.f, 1.f }; // Y-up for standard FPS
	glm::vec3 camRight;
	void UpdateCamera();

	glm::mat4 getViewMatrix();
	glm::mat4 getProjectionMatrix();

	// Game Object
	void populateObjectBuffer(ObjectBuffer& buffer, std::vector<Vertex> vertices, std::vector<uint32_t> indices);
	std::vector<GameObject*> gameObjects;
	int objectsAllocated = 0;
	void cleanupGameObject(GameObject* object);

	// Resource Management
	std::vector<Texture*> textures;
	std::vector<Mesh*> meshes;
	std::vector<Sound*> sounds;
	std::vector<Trigger*> triggers;
	std::vector<CharacterController*> charControllers;

	// Image
	void createTextureSampler();
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	void createTextureImageView(VkImageView& textureImageView, VkImage& textureImage);
	void createTextureImage(const char* texName, VkImage& texture, VkDeviceMemory& texMem);
	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	// Single Time Commands
	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);

	// Depth Resource
	VkFormat findDepthFormat();
	bool hasStencilComponent(VkFormat format);
	void createDepthResources();

	// Descriptor Sets
	void createDescriptorPool();
	void createDescriptorSetLayouts();
	void createFrameDescriptorSets();

	void createUniformBuffers();
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	// Buffers
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void createVertexBuffer(VkBuffer& buffer, VkDeviceMemory& bufferMemory, const std::vector<Vertex>& bufferData);
	void createIndexBuffer(VkBuffer& buffer, VkDeviceMemory& bufferMemory, const std::vector<uint32_t>& bufferData);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	// Swapchain and Graphics Pipeline
	void cleanupSwapChain();
	void recreateSwapChain();
	static std::vector<char> readFile(const std::string& filename);
	VkShaderModule createShaderModule(const std::vector<char>& code);
	void createGraphicsPipeline();
	void createImageViews();
	void createSwapChain();

	void createSyncObjects();

	// Command Buffer
	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void createCommandBuffer();
	void createCommandPool();

	// Framebuffers and Renderpasses
	void createFramebuffers();
	void createRenderPass();

	// Surface
	void createSurface();

	// Devices
	void createLogicalDevice();
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	bool isDeviceSuitable(VkPhysicalDevice device);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	void pickPhysicalDevice();

	// Instance
	void createInstance();

	// Debug Layers
	void setupDebugMessenger();
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

	// Helpers
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

	// Core functions
	void drawFrame();
	uint32_t currentFrame = 0;
	void updateUniformBuffer(uint32_t currentImage);
	bool exitFlag = false;

	// descriptorsets.cpp
	size_t dynamicAlignment;

	// PhysX
	bool visualizePhysX = false;
	PxDefaultAllocator      gAllocator;
	PxDefaultErrorCallback  gErrorCallback;
	PxFoundation* gFoundation = nullptr;
	PxPhysics* gPhysics = nullptr;
	PxScene* gScene = nullptr;
	PxMaterial* gDefaultMaterial = nullptr;
	PhysicsMaterial* eDefaultMaterial = nullptr;
#ifndef _WIN32
	PxCooking* mCooking;
#endif
	std::deque<PhysicsMaterial> materials;

	ControllerHitCallback* mHitCallback;
	PhysicsEventListener* mPhysicsEventListener;

	void initPhysX();
	PxRigidStatic* createStaticActor(Mesh* mesh, Vector3 scale, PxMaterial* material);
	PxRigidDynamic* createDynamicActor(Mesh* mesh, Vector3 scale, PxMaterial* material);
	void cookMesh(Mesh* mesh);
	void updatePhysics(float deltaTime);

	PxControllerManager* gControllerManager;

	// FMOD Sound API
	FMOD::System* system = nullptr;
	void initFMOD();
	void syncListenerPos();
	int framesToUnmute = 3;
	bool masterMuted = true;
	bool globalMute = false;

	// imgui
	VkDescriptorPool imguiPool;
	std::function<void(Engine* engine)> uiCallback = nullptr;
	void InitImGui(
		GLFWwindow* window,
		VkInstance instance,
		VkPhysicalDevice physicalDevice,
		VkDevice device,
		VkQueue graphicsQueue,
		VkRenderPass renderPass,
		uint32_t imageCount
	);

	std::unordered_map<std::string, IResource*> resources;

	// Scene Management
	ENGINE_API bool loadScene_internal(Scene* scene, const char* sceneFile);
	void cleanupState();
	Scene* activeScene = nullptr;
	Scene* sceneToLoad = nullptr;
	bool shouldLoadScene = false;
	std::unordered_map<std::string, Scene*> sceneMap;

	// Engine UI Layer
	void initUILayer();
	//void cleanupUILayer();
	std::vector<UIElement*> uiElements;
	Mesh* uiQuad = nullptr;

	// === DUALSENSE ===

	#ifdef _WIN32

	// FMOD DualSense haptics
	FMOD::ChannelGroup* hapticGroup = nullptr;
	static FMOD_RESULT PCMGrabDSP(FMOD_DSP_STATE* state, float* inbuffer, float* outbuffer,
		unsigned int length, int inchannels, int* outchannels);
	void initHaptics();

	// WASAPI
	static std::vector<float> pcmBuffer;
	static std::atomic<size_t> writePos;
	static std::atomic<size_t> readPos;

	IMMDeviceEnumerator* enumerator = nullptr;
	IMMDeviceCollection* collection = nullptr;
	IMMDevice* immDevice = nullptr;
	IAudioClient* audioClient = nullptr;
	IAudioRenderClient* renderClient = nullptr;
	HANDLE hAudioEvent = nullptr;
	UINT32 bufferFrameCount = 0;

	void AudioRenderThread(); // dsp sync
	std::thread mAudioThread;

	// Dualsense RGB
	void initDSRGB();
	HANDLE dsHID;
	bool dsPresent = false;

	#endif

	// === END DUALSENSE ===

	// Gamepad support
	void readGlfwGamePadState();
	GamepadState gamepadState;

	// debug
	std::vector<RayDebug> gRayDebugs;
	void renderPhysXDebug(const glm::mat4& viewProjMatrix, float screenWidth, float screenHeight);
};

// misc
glm::vec3 Vec3toGlm(Vector3& v);