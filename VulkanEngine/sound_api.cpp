#include "engine.h"

void Engine::initFMOD() {
    FMOD::System_Create(&system);
    system->init(512, FMOD_INIT_3D_RIGHTHANDED, nullptr);
    system->set3DSettings(0.0f, 1.0f, 1.0f); // no doppler yet
}

void Engine::syncListenerPos() {
    FMOD_VECTOR listenerPos = { cameraPosition.x, cameraPosition.y, cameraPosition.z };
    FMOD_VECTOR listenerVel = { 0.0f, 0.0f, 0.0f };

    FMOD_VECTOR listenerForward = { camFront.x, camFront.y, camFront.z };

    glm::vec3 worldUp = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 right = glm::normalize(glm::cross(camFront, worldUp));

    glm::vec3 actualUp = glm::cross(right, camFront);

    FMOD_VECTOR listenerUp = { actualUp.x, actualUp.y, actualUp.z };

    system->set3DListenerAttributes(0, &listenerPos, &listenerVel, &listenerForward, &listenerUp);
}

Sound* Engine::createSound(std::string name, const char* path, bool looping, bool three_dim) {
    if (resources.contains(name))
    {
        throw std::runtime_error("Resource name already used: " + name);
    }

    Sound* snd = new Sound();
    snd->name = name;
    sounds.push_back(snd);

    auto mode = three_dim ? FMOD_3D : FMOD_2D;
    if (looping) mode = mode | FMOD_LOOP_NORMAL;
    auto result = system->createSound(path, mode, nullptr, &snd->sound);

    if (result != FMOD_OK) {
        printf("FMOD Error (%d)\n", result);

        char errorBuffer[1024];
        sprintf_s(errorBuffer, 1024, "Failed to load sound %s", path);
        OnError_Handler(errorBuffer);
    }

    resources[name] = snd;

    return snd;
}

void Engine::setGlobalMute(bool mute) {
    FMOD::ChannelGroup* master;
    system->getMasterChannelGroup(&master);
    master->setMute(mute);
    globalMute = mute;
}

void Engine::playSound(Sound* sound, FMOD::ChannelGroup* group, FMOD::Channel** channel, bool startPaused) {
    system->playSound(sound->sound, group, startPaused, channel);
}

Sound::Sound() {
    sound = nullptr;
};

void Sound::destroy(void* ptr) {
    sound->release();
}

ResourceType Sound::getType() {
    return SOUND;
}