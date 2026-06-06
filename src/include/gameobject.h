#pragma once

#include <vulkan/vulkan.h>
#include "engine_types.h"
#include <glm/glm.hpp>

#include <PxPhysicsAPI.h>
#include <fmod.hpp>

#include "sound.h"

#include <functional>

#include <string>

#include "texture.h"

class Engine;

class ENGINE_API GameObject {
	friend class Engine;

public:
	GameObject();

	virtual ~GameObject() {}

	// Transform
	Transform transform;
	void playSound(Sound* sound, float volume);
	void stopAllSounds();
	void setSoundPause(bool pause);

	void applyForce(Vector3 direction, float power);
	void applyForce(const Vector3& force);
	Vector3 getVelocity();

	void updateTexture(Texture* newTexture);

	std::function<void(GameObject* other, float impulse)> onCollision;

	std::string name;
	std::string tag;

	void setPhysicsType(PhysicsType type);
	void updateTransform();

	uint32_t getID();

	virtual void Start();
	virtual void Update();
private:
	bool skip = false;
	
	glm::mat4 GetModel();

	// Engine Internal objects
	UniformBufferObject uboData;
	ObjectBuffer objBuffer;

	// PhysX stuff
	physx::PxRigidActor* physicsActor = nullptr;
	bool isDynamic = false;

	FMOD::ChannelGroup* channelGroup;

	Engine* engPtr;
	Texture* texture;

	int indexCount;

	void updateSound();

	uint32_t id;
};

class ENGINE_API Trigger {
	friend class Engine;
public:
	std::function<void(GameObject* other)> onTriggerEnter;
	std::function<void(GameObject* other)> onTriggerExit;
private:
	physx::PxRigidActor* physicsActor = nullptr;
};