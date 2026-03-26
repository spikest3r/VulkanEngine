#include "gameobject.h"
#include "engine.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// Conversion Helper
FMOD_VECTOR vecToFmod(const Vector3& v) {
    return { v.x, v.y, v.z };
}

GameObject::GameObject() {
    transform.position = { 0.0f,0.0f,0.0f };
    transform.rotation = { 0.0f,0.0f,0.0f,0.0f };
    transform.scale = { 1.0f,1.0f,1.0f };
}

glm::mat4 GameObject::GetModel() {
    glm::mat4 baseModel = glm::mat4(1.0f);

    // 1. Translation (World Space)
    baseModel = glm::translate(baseModel, Vec3toGlm(transform.position));

    // 2. Rotation (Using Quaternion)
    // toGlm() handles the (w, x, y, z) ordering for you
    baseModel *= glm::mat4_cast(glm::quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w));

    // 3. Scale (Local Space)
    baseModel = glm::scale(baseModel, Vec3toGlm(transform.scale));

    return baseModel;
}

void Engine::internal_createGameObject(
    GameObject* ptr,
    Transform spawnTransform,
    Mesh* mesh,
    Texture* texture,
    PhysicsMaterial* material,
    bool isDynamic
) {
    // Initialize (Overwrite old data)
    ptr->transform = spawnTransform;
    if(mesh) {
        ptr->objBuffer.vertexBuffer = mesh->verticesVk;
        ptr->objBuffer.indexBuffer = mesh->indicesVk;
        ptr->indexCount = static_cast<uint32_t>(mesh->indices.size());
        ptr->isDynamic = isDynamic;
    } else {
        ptr->isDynamic = false;
    }
    ptr->engPtr = this;
    ptr->name = "Game object";
    ptr->tag = "Default tag";
    ptr->id = gameObjectID++;

    // Vulkan & FMOD
    if(mesh) createGameObjectDescriptorSet(*ptr, texture->imageView);

    // Reuse or create ChannelGroup
    std::string groupName = "ObjGroup_" + std::to_string(gameObjectID); // Use index as ID
    system->createChannelGroup(groupName.c_str(), &ptr->channelGroup);
    ptr->channelGroup->setMode(FMOD_2D);

    // Physics
    if (isDynamic && mesh) {
        ptr->physicsActor = createDynamicActor(mesh, spawnTransform.scale, material->material);
    }
    else {
        ptr->physicsActor = createStaticActor(mesh, spawnTransform.scale, material->material);
    }

    if (ptr->physicsActor) {
        PxTransform pxTrans(PxVec3(ptr->transform.position.x, ptr->transform.position.y, ptr->transform.position.z));
        ptr->physicsActor->setGlobalPose(pxTrans);
        ptr->physicsActor->userData = ptr;

        if (isDynamic) {
            physx::PxRigidDynamic* dyn = static_cast<physx::PxRigidDynamic*>(ptr->physicsActor);
            dyn->setMass(10.0f);

            float inertiaVal = (1.0f / 6.0f) * 10.0f * (1.0f * 1.0f);
            dyn->setMassSpaceInertiaTensor(PxVec3(inertiaVal, inertiaVal, inertiaVal));

            dyn->setAngularDamping(1.0f);
            dyn->setLinearDamping(0.5f);
            dyn->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
            dyn->wakeUp();
        }
    }

    if(!mesh) ptr->skip = true;

    gameObjects.push_back(ptr);

    updateUniformBuffer(currentFrame);
}

void GameObject::updateTexture(Texture* newTexture) {
    engPtr->updateGameObjectDescriptorSet(*this, newTexture->imageView);
}

void GameObject::playSound(Sound* sound, float volume)
{
    FMOD::Channel* channel = nullptr;

    engPtr->playSound(sound, channelGroup, &channel, false);

    if (!channel)
        return;

    channel->setVolume(volume);

    FMOD_VECTOR pos = vecToFmod(transform.position);
    channel->set3DAttributes(&pos, nullptr);
}

void GameObject::stopAllSounds() {
    channelGroup->stop();
}

void GameObject::updateSound()
{
    if (!channelGroup)
        return;

    FMOD_VECTOR pos = vecToFmod(transform.position);
    FMOD_VECTOR vel = {0.0f, 0.0f, 0.0f};

    if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z))
        return;

    channelGroup->set3DAttributes(&pos, &vel);
}

void GameObject::setSoundPause(bool pause) {
    if (channelGroup) channelGroup->setPaused(pause);
}

void GameObject::applyForce(Vector3 direction, float power) {
    if (physicsActor) {
        physx::PxRigidBody* body = physicsActor->is<physx::PxRigidBody>();

        if (body) {
            physx::PxVec3 force = { direction.x * power, direction.y * power, direction.z * power };

            body->addForce(force, physx::PxForceMode::eIMPULSE);
        }
    }
}

Vector3 GameObject::getVelocity() {
    physx::PxRigidDynamic* dyn = physicsActor->is<physx::PxRigidDynamic>();
    physx::PxVec3 vel = dyn->getLinearVelocity();
    return {vel.x,vel.y,vel.z};
}

void GameObject::applyForce(const Vector3& force)
{
    if (physicsActor)
    {
        physx::PxRigidBody* body = physicsActor->is<physx::PxRigidBody>();

        if (body)
        {
            physx::PxVec3 pxForce(force.x, force.y, force.z);
            body->addForce(pxForce, physx::PxForceMode::eFORCE);
        }
    }
}

Trigger* Engine::createBoxTrigger(Vector3 pos, Vector3 size) {
    PxRigidStatic* actor = gPhysics->createRigidStatic(PxTransform(pos.x, pos.y, pos.z));
    PxBoxGeometry geometry(size.x / 2.0f, size.y / 2.0f, size.z / 2.0f);
    PxShape* shape = PxRigidActorExt::createExclusiveShape(*actor, geometry, *gDefaultMaterial);
    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
    shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
    gScene->addActor(*actor);

    Trigger* newTrig = new Trigger();
    newTrig->physicsActor = actor;

    triggers.push_back(newTrig);

    actor->userData = newTrig;

    return newTrig;
}

void Engine::requestDestroyGameObject(GameObject* object) {
    gameObjectDestroyQueue.push(object);
}

void Engine::checkGameObjectDestroy() {
    if(gameObjectDestroyQueue.empty()) return;

    while (!gameObjectDestroyQueue.empty()) {
		auto& object = gameObjectDestroyQueue.front();
        
        if(object) {
            cleanupGameObject(object);
        }

        gameObjectDestroyQueue.pop();
    }
}

inline ObjectHeader* getHeader(void* obj) {
    return (ObjectHeader*)((char*)obj - sizeof(ObjectHeader));
}

void Engine::cleanupGameObject(GameObject* object)
{
    if (!object) return;

    // 1. stop FMOD FIRST
    if (object->channelGroup)
    {
        object->channelGroup->stop();
        object->channelGroup->release();
        object->channelGroup = nullptr;
    }

    object->stopAllSounds();

    // 2. physics
    if (object->physicsActor)
    {
        gScene->removeActor(*object->physicsActor);
        object->physicsActor->release();
    }

    // 3. remove from vector
    for (size_t i = 0; i < gameObjects.size(); i++)
    {
        if (gameObjects[i] == object)
        {
            gameObjects[i] = gameObjects.back();
            gameObjects.pop_back();
            break;
        }
    }

    // 4. Vulkan cleanup
    vkFreeDescriptorSets(device, descriptorPool, 1, &object->descriptorSet);

    // 5. memory free LAST
    ObjectHeader* h = getHeader(object);
    h->destroy(object);
}

void GameObject::setPhysicsType(PhysicsType type) {
    if (!physicsActor) return;

    physx::PxRigidDynamic* dynamicActor = physicsActor->is<physx::PxRigidDynamic>();

    if (type == PhysicsType::Static) {
        isDynamic = false;
        if (dynamicActor) {
            dynamicActor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, false);
        }
    } 
    else if (type == PhysicsType::Kinematic) {
        isDynamic = true;
        if (dynamicActor) {
            dynamicActor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
        }
    } 
    else if (type == PhysicsType::Dynamic) {
        isDynamic = true;
        if (dynamicActor) {
            dynamicActor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, false);
        }
    }
}

void GameObject::setPosition(Vector3 position) {
    if (!physicsActor) return;

    transform.position = position;

    physx::PxTransform newPose(physx::PxVec3(position.x, position.y, position.z), physicsActor->getGlobalPose().q);

    physx::PxRigidDynamic* dynamicActor = physicsActor->is<physx::PxRigidDynamic>();
    
    if (dynamicActor && (dynamicActor->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC)) {
        dynamicActor->setKinematicTarget(newPose);
    } else {
        physicsActor->setGlobalPose(newPose);
    }
}

uint32_t GameObject::getID() {return id;}

void GameObject::Update() {

}

void GameObject::Start() {

}