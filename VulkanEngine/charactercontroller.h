#pragma once
#include "engine_types.h"

class Engine;

class ICharacterController : public GameObject {
public:
	virtual ~ICharacterController() {}
	virtual void Move(Vector3 direction, float speed, float dt) = 0;
	virtual void Jump(float force) = 0;
	virtual Vector3 getPosition() = 0;
	virtual void setPosition(Vector3 position) = 0;
	virtual float getVerticalVelocity() = 0;
};

class CharacterController : public ICharacterController {
	friend class Engine;
public:
	void Move(Vector3 dir, float s, float dt) override;
	void Jump(float f) override;
	Vector3 getPosition() override;
	void setPosition(Vector3 position) override;
	float getVerticalVelocity() override;
private:
	physx::PxController* playerController; // Hidden from Game
	float verticalVelocity = 0.0f;
	const float gravity = -24.0f;
};