#pragma once

#include "engine_types.h"
#include "sound.h"
#include <functional>
#include "texture.h"

class Engine;

class ENGINE_API GameObject {
public:
	GameObject();

	virtual ~GameObject() {}

	// Transform
	Transform transform;

	void playSound(Sound* sound, float volume);
	void stopAllSounds();
	void setSoundPause(bool pause);

	void applyForce(Vector3 direction, float power);

	void updateTexture(Texture* newTexture);

	std::function<void(GameObject* other, float impulse)> onCollision;

	std::string name;
};

class ENGINE_API Trigger {
public:
	std::function<void(GameObject* other)> onTriggerEnter;
	std::function<void(GameObject* other)> onTriggerExit;
};