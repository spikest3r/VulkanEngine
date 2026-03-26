#include "engine.h"
#include <iostream>
#include "engine_types.h"
#include "shapes.h"
#include <cstring>
#include <fstream>
#include "engine_ui.h"

#ifndef _WIN32
#include <unistd.h>
#endif

static Engine* engine;
static PhysicsMaterial* mat;
static bool uiVisible = false;
static float usageMB = 0.0f;

inline float length(Vector3 vector) {
	return sqrt(vector.x*vector.x + vector.y*vector.y + vector.z*vector.z);
}

class Puppet : public GameObject {
public:
	void ToggleTexture();
	void Start() override;
public:
	bool tex = false;
};

class GameScene : public Scene {
public:
	Sound* magic;
	Sound* success;
	Sound* step1;
	Sound* step2;
	Sound* spawn;
	Sound* disappear;
	Sound* destroy;

	ICharacterController* controller;
	Trigger* respawnTrigger;
	Trigger* portalTrigger;
	Trigger* triggerSuccess1;

	void SetMouseFirst() { firstMouse = true; }
	GameObject* createPuppet();
	void clearPuppets();

	std::vector<GameObject*> puppets;
private:
	// --- Player & Physics Setup ---
	const float moveSpeed = 12.0f;
	const float mouseSensitivity = 0.1f;
	float pitch = 0.0f;
	float yaw = -90.0f;
	const float eyeHeight = 1.7f; // Camera offset from controller feet

	bool firstMouse = true;
	float lastX = 0.0f, lastY = 0.0f;
	bool spaceKey = false;
	

	bool spawnPressed = false;
	bool clearPressed = false;
	bool reloadPressed = false;

	Vector3 prevCameraPos = { 0,0,0 };
	bool initialized = false;
	float bobTime = 0.0f;

	// --- bobbing tuning ---
	float bobbingAmplitude = 0.2f;
	float bobbingFrequency = 13.0f;
	float bobbingSmoothness = 10.0f;
	float bobbingMaxSpeed = 5.0f;
	float bobbingBaseZ = 0.0f;
	float previousCosBob = 0;
	float lastVerticalVel = 0.0f;
	bool isGrounded = false;

	bool stepCount = false;

	bool isMovingObject = false;
	GameObject* objectToMove = nullptr;
	float movingDistance = 10.0f;

	GameObject* CreateGameObject(Engine* engine, const char* objectType, const char* tag, const char* name, Transform transform, Mesh* mesh, Texture* texture, bool dynamic) override;
	void InitScene(Engine* engine) override;
	void UpdateScene(Engine* engine) override;
	void DestroyScene(Engine* engine) override;
};

GameObject* GameScene::CreateGameObject(Engine* engine, const char* objectType, const char* tag, const char* name, Transform transform, Mesh* mesh, Texture* texture, bool dynamic) {
	GameObject* object;
	if (!strcmp(objectType, "Puppet")) {
		object = engine->createGameObject<Puppet>(transform, mesh, texture, mat, dynamic);
		object->tag = "Puppet";
		puppets.push_back(object);
	}
	else {
		object = engine->createGameObject<GameObject>(transform, mesh, texture, mat, dynamic);
		object->tag = tag;
	}

	object->name = name;

	return object;
}

void GameScene::InitScene(Engine* engine) {
	std::cout << "Creating game scene" << std::endl;

	magic = engine->createSound("MagicSFX", "magic.mp3", false, true);
	success = engine->createSound("SuccessSFX", "success.mp3", false, false);
	step1 = engine->createSound("StepSFX1", "step1.wav", false, false); // step 0
	step2 = engine->createSound("StepSFX2", "step2.wav", false, false); // step 1
	spawn = engine->createSound("SpawnSound", "spawn.wav", false, false);
	destroy = engine->createSound("DestroySound", "disappear.wav", false, false);

	Vector3 playerStart = { -7.5f, -5.5f, 5.0f };
	PhysicsMaterial* material = engine->createPhysicsMaterial(0.0f, 0.0f, 0.0f); // Low friction for CCT
	controller = engine->createCharacterController(1.8f, 0.3f, playerStart, material, true);

	Mesh* cubeMesh = engine->getMesh("CubeMesh");
	Texture* cubeTex = engine->getTexture("CubeTex1");
	Texture* cubeTex2 = engine->getTexture("CubeTex2");

	respawnTrigger = engine->createBoxTrigger({ 0.0f,0.0f,-15.0f }, { 300.0f,300.0f,3.0f });
	respawnTrigger->onTriggerEnter = [this, engine](GameObject* other) {
		if (other->tag == "Character Controller") {
			std::cout << "Respawn " << other->name << std::endl;
			controller->setPosition({ 0.0f,0.0f,15.0f });
		}
		else {
			std::cout << "Destroy " << other->name << std::endl;

			for (size_t i = 0; i < puppets.size(); i++) {
				if (puppets[i] == other) {
					engine->requestDestroyGameObject(puppets[i]);

					puppets[i] = puppets.back();
					puppets.pop_back();
					break;
				}
			}
		}
	};

	portalTrigger = engine->createBoxTrigger({ -13.35f,7.05f,4.65f }, { 1.22f,2.8f,3.5f });
	bool portalTextureState = false;
	portalTrigger->onTriggerEnter = [this, cubeTex, cubeTex2, &portalTextureState, engine](GameObject* other) {
		if (other->tag == "Puppet") {
			assert(other != nullptr);
			auto ptr = dynamic_cast<Puppet*>(other);
			if (ptr) {
				ptr->playSound(magic, 1.0f);
				engine->playHaptics(magic, 1.0f);
				ptr->ToggleTexture();
			}
		}
	};

	triggerSuccess1 = engine->createBoxTrigger({ 13.f,13.f,13.f }, { 2.7f,2.7f,2.7f });
	triggerSuccess1->onTriggerEnter = [engine/*, scene*/](GameObject* other) {
		assert(other != nullptr);

		std::cout << "Entered triggerSuccess1" << std::endl;
		std::cout << "Name: " << other->name << "\nTag: " << other->tag << "\nID: " << other->getID() << std::endl << std::endl;

		//scene->playSound(success, 1.0f);

		engine->addTimer(2.0f, []()
			{
				std::cout << "TODO: USE TIMERS\n";
			});
		};
}

// all requestDestroy... calls are forced in this scope, destroy is guranteed immediately
void GameScene::DestroyScene(Engine* engine) {
	// here we destroy other resources (triggers, ctrls etc)
	// all sounds textures meshes and gameobjects are cleared after DestroyScene()

	std::cout << "Destroying scene..." << std::endl;

	engine->requestDestroyTrigger(respawnTrigger);
	engine->requestDestroyTrigger(portalTrigger);
	engine->requestDestroyTrigger(triggerSuccess1);

	engine->requestDestroyCharacterController(controller);
}

void GameScene::UpdateScene(Engine* engine) {
	float dt = engine->getDeltaTime();

	GamepadState* state = engine->getGamepad();

	Vector3 forward, right;

	float lookX = state->axes[GAMEPAD_AXIS_RIGHT_X];
	float lookY = state->axes[GAMEPAD_AXIS_RIGHT_Y];

	const float lookDeadzone = 0.12f;
	if (abs(lookX) < lookDeadzone) lookX = 0.0f;
	if (abs(lookY) < lookDeadzone) lookY = 0.0f;

	float stickSensitivity = 120.0f;

	yaw -= lookX * stickSensitivity * dt;
	pitch -= lookY * stickSensitivity * dt;

	// clamp pitch
	if (pitch > 89.0f)  pitch = 89.0f;
	if (pitch < -89.0f) pitch = -89.0f;

	// apply camera rotation
	engine->cameraRotation.x = pitch;
	engine->cameraRotation.y = yaw;

	// update camera basis vectors
	engine->getCameraVectors(forward, right);

	float moveX = state->axes[GAMEPAD_AXIS_LEFT_X];
	float moveY = state->axes[GAMEPAD_AXIS_LEFT_Y];

	const float deadzone = 0.15f;
	if (abs(moveX) < deadzone) moveX = 0.0f;
	if (abs(moveY) < deadzone) moveY = 0.0f;

	Vector3 wishDir = { 0, 0, 0 };
	wishDir += forward * -moveY;
	wishDir += right * moveX;

	// apply movement to character controller
	controller->Move(wishDir, moveSpeed, dt);

	Vector3 cPos = controller->getPosition();
	engine->cameraPosition = { cPos.x, cPos.y, cPos.z + eyeHeight };

	Vector3 rayOrigin = engine->cameraPosition;
	Vector3 rayDir = forward;

	// movement effects
	float currentVel = controller->getVerticalVelocity();

	const float velThreshold = 0.5f;

	if (abs(currentVel) < velThreshold) {
		if (!isGrounded) isGrounded = true;
	}
	else {
		isGrounded = false;
	}

	lastVerticalVel = currentVel;

	float targetZ = 0.0f;
	Vector3 currentPos = engine->cameraPosition;
	if (isGrounded) {
		if (!initialized)
		{
			prevCameraPos = currentPos;
			initialized = true;
		}

		// --- velocity from camera position ---
		Vector3 velocity = (currentPos - prevCameraPos) / dt;

		// ignore Z (up axis)
		float speed = sqrt(velocity.x * velocity.x + velocity.y * velocity.y);

		// normalize speed
		float speedFactor = speed / bobbingMaxSpeed;
		if (speedFactor > 1.0f) speedFactor = 1.0f;

		// --- update bob time ---
		if (speed > 0.05f)
		{
			bobTime += dt * bobbingFrequency * speedFactor;
		}

		// --- compute bob ---
		float bob = sin(bobTime);
		float bobOffset = bob * bobbingAmplitude * speedFactor;

		// --- target Z ---
		targetZ = bobbingBaseZ;
		if (speed > 0.05f)
			targetZ += bobOffset;
	}
	else {
		targetZ = 0.0f;
	}

	// --- smooth apply to cameraOffset.z ---
	engine->cameraOffset.z += (targetZ - engine->cameraOffset.z) * dt * bobbingSmoothness;

	float cosBob = cos(bobTime);
	if (cosBob < 0 && previousCosBob >= 0) {
		controller->playSound(stepCount ? step2 : step1, 1.0f);
		stepCount = !stepCount;
	}
	previousCosBob = cosBob;

	prevCameraPos = currentPos;

	auto keyState = state->axes[GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.5f;
	if (keyState && !isMovingObject) {
		float maxDistance = 50.0f;
		RaycastHit hit = engine->raycast(rayOrigin, rayDir, maxDistance);

		if (hit.object) {
			if (hit.object->tag == "Puppet") {
				isMovingObject = true;
				objectToMove = hit.object;
				movingDistance = hit.distance > 10.0f ? hit.distance : 10.0f;
			}
		}
	}
	else if (!keyState && isMovingObject) {
		isMovingObject = false;
		objectToMove = nullptr;
	}

	if (isMovingObject && objectToMove) {
		try {
			Vector3 target = currentPos + forward * movingDistance;

			Vector3 objectPos = objectToMove->transform.position;
			Vector3 dir = target - objectPos;

			Vector3 vel = objectToMove->getVelocity();

			Vector3 force = dir * 180.0f - vel * 25.0f;

			objectToMove->applyForce(force);
		}
		catch (std::exception) {
			objectToMove = nullptr;
			isMovingObject = false;
		}
	}

	if (state->buttons[GAMEPAD_BUTTON_CROSS]) {
		if (!spaceKey) {
			spaceKey = true;
			controller->Jump(15.0f);
		}
	}
	else {
		spaceKey = false;
	}

	if (state->buttons[GAMEPAD_BUTTON_SQUARE]) {
		if (!spawnPressed) {
			spawnPressed = true;

			auto scene = static_cast<GameScene*>(engine->getActiveScene());

			GameObject* obj = scene->createPuppet();

			Vector3 forward, right;
			engine->getCameraVectors(forward, right);

			Vector3 camPos = engine->cameraPosition;

			Vector3 spawnPos = camPos + forward * 1.5f;

			obj->setPosition(spawnPos);

			// shoot forward
			obj->applyForce(forward, 600.0f);

			engine->playHaptics(spawn, 0.8f);
		}
	}
	else {
		spawnPressed = false;
	}

	if (state->buttons[GAMEPAD_BUTTON_LEFT_BUMPER]) {
		if (!clearPressed) {
			clearPressed = true;

			auto scene = static_cast<GameScene*>(engine->getActiveScene());

			scene->clearPuppets();

			engine->playHaptics(destroy, 1.0f);
		}
	}
	else {
		clearPressed = false;
	}

	if (state->buttons[GAMEPAD_BUTTON_START]) {
		if (!reloadPressed) {
			reloadPressed = true;

			auto scene = engine->getActiveScene();

			engine->loadScene(scene);
		}
	}
	else {
		reloadPressed = false;
	}

	//if (engine->getKey(KeyCode::X) == PRESS) {
	//	if (!xKey) {
	//		xKey = true;
	//		uiVisible = !uiVisible;
	//		engine->setCursorMode(uiVisible ? NORMAL : DISABLED);
	//		GameScene* currentScene = static_cast<GameScene*>(engine->getActiveScene());
	//		currentScene->SetMouseFirst(); // firstMouse = true;
	//	}
	//}
	//else {
	//	xKey = false;
	//}
}

void Puppet::Start() {
	std::cout << "Puppet created!" << std::endl;
}

void Puppet::ToggleTexture() {
	tex = !tex;
	Texture* texture = engine->getTexture(tex ? "CubeTex2" : "CubeTex1");
	if(texture) {
		updateTexture(texture);
	}
}

GameObject* GameScene::createPuppet() {
	Mesh* cubeMesh = engine->getMesh("CubeMesh");
	Texture* cubeTex = engine->getTexture("CubeTex1");
	Transform cubeTrans = { {0.0f,0.0f,15.0f},{0.0f,0.0f,0.0f,0.0f},{1.0f,1.0f,1.0f} };
	GameObject* newObject = engine->createGameObject<Puppet>(cubeTrans, cubeMesh, cubeTex, mat, true);
	newObject->tag = "Puppet";
	newObject->name = "Instance of Cube";
	puppets.push_back(newObject);
	return newObject;
}

void GameScene::clearPuppets() {
	for (auto& puppet : puppets) {
		engine->requestDestroyGameObject(puppet);
	}
	puppets.clear();
}

void MainUI() {
	// if(uiVisible) GameUI();

	UI::Begin("Engine Monitor");
    
    auto vramData = engine->getVRAMStats();
    for (const auto& heap : vramData) {
		char buffer1[128];
		sprintf_s(buffer1, 128, "VRAM Heap %u", heap.heapIndex);
		UI::Text(buffer1);
		UI::ProgressBar(heap.usageMB / heap.budgetMB, Vector2(0.0f, 0.0f));
		UI::SameLine();
		char buffer2[128];
		sprintf_s(buffer2, 128, "%.1f / %.1f MB", heap.usageMB, heap.budgetMB);
		UI::Text(buffer2);
    }
    
	UI::End();
}

int main() {
	engine = Engine::Create();

	try {
		engine->init(800, 600, "Arch Game");
	}
	catch (const std::exception& e) {
		std::cout << "Exception caught!" << std::endl;
		std::cout << e.what() << std::endl;
		Engine::Destroy(engine);
		return EXIT_FAILURE;
	}

	engine->setCursorMode(DISABLED);

	mat = engine->createPhysicsMaterial(0.5f, 0.4f, 0.2f);

	Scene* scene = engine->createScene<GameScene>("D:\\engine\\VulkanEngine-65b374bc2e12a985c3385b342ba707b8312c78aa\\VulkanEngine\\scene.txt", nullptr);

	engine->loadScene(scene);

	engine->setClearColor({0.039f, 0.102f, 0.200f});
	engine->SetUICallback(MainUI);

	Vector3 forward = { 0.0f, 0.0f, 0.0f };
	Vector3 right = { 0.0f, 0.0f, 0.0f };

	while (engine->running()) {
		engine->update();

		float dt = engine->getDeltaTime();

		if (engine->getKey(KeyCode::Escape) == PRESS) engine->exit();

		engine->updateScene();

		engine->render();
	}
	engine->cleanup();

	Engine::Destroy(engine);
	return EXIT_SUCCESS;
}