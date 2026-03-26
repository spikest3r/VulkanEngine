#pragma once
#include <fmod.hpp>

class Engine;
class GameObject;

class ENGINE_API Sound : public IResource {
	friend class GameObject;
	friend class Engine;
public:
	Sound();
private:
	FMOD::Sound* sound;
	void destroy(void*) override;
	ResourceType getType() override;
};