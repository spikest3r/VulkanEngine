#pragma once
#include "3d_loader.h"
#include "engine_types.h"
#include "gameobject.h"
#include "texture.h"
#include "sound.h"
#include "charactercontroller.h"

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

	static ENGINE_API Engine* Create(); // Static factory
	static ENGINE_API void Destroy(Engine* instance);

	// Game Object
	ENGINE_API GameObject* createGameObject(
		Transform spawnTransform,
		Mesh* mesh,
		Texture* texture,
		PhysicsMaterial* material,
		bool isDynamic
	);

	ENGINE_API Texture* allocateTexture(const char* path);
	ENGINE_API Mesh* loadModel(const char* path);
	ENGINE_API Mesh* createMesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices);
	ENGINE_API Sound* createSound(const char* path, bool looping, bool three_dim);

	ENGINE_API KeyState getKey(KeyCode code);

	ENGINE_API Vector2 getMousePos();

	Vector3 cameraPosition = { 0.0f, 0.0f, 5.0f }; // Start 5 units back
	Vector3 cameraRotation = { 0.0f, -90.0f, 0.0f }; // Point toward the scene

	ENGINE_API ICharacterController* createCharacterController(float height, float radius, Vector3 position, PhysicsMaterial* material, bool interactWithActors);
	ENGINE_API PhysicsMaterial* createPhysicsMaterial(float staticFriction, float dynamicFriction, float restitution);

	ENGINE_API Trigger* createBoxTrigger(Vector3 pos, Vector3 size);

	ENGINE_API void setGlobalMute(bool mute);

	ENGINE_API void requestDestroy(IResource* resource);
	ENGINE_API void destroyGameObject(GameObject* object);
};
