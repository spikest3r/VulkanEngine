#include "engine.h"

#include <characterkinematic/PxController.h>
#include <characterkinematic/PxControllerManager.h>

void ControllerHitCallback::onShapeHit(const PxControllerShapeHit& hit) {
    PxRigidActor* rigidActor = hit.actor;
    if (!rigidActor) return;

    PxRigidDynamic* actor = rigidActor->is<PxRigidDynamic>();

    // Safety checks
    if (!actor || (actor->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC)) return;
    if (!actor->getScene()) return;

    PxVec3 pushDir = hit.dir;
    float pushStrength = 50.0f;

    PxVec3 forcePos = PxVec3(
        static_cast<float>(hit.worldPos.x),
        static_cast<float>(hit.worldPos.y),
        static_cast<float>(hit.worldPos.z)
    );

    PxRigidBodyExt::addForceAtPos(*actor,
        pushDir * pushStrength,
        forcePos,
        PxForceMode::eIMPULSE);
}

ICharacterController* Engine::createCharacterController(float height, float radius, Vector3 position, PhysicsMaterial* material, bool interactWithActors) {
	PxCapsuleControllerDesc desc;
	desc.height = height;
	desc.radius = radius;
	desc.stepOffset = 0.0f;
	desc.material = material->material;
	desc.position = PxExtendedVec3(position.x,position.y,position.z);
	desc.upDirection = PxVec3(0, 0, 1); // Z+
    if(interactWithActors) desc.reportCallback = mHitCallback;

    CharacterController* newCtrl = new CharacterController();

	PxController* playerController = gControllerManager->createController(desc);

    newCtrl->playerController = playerController;

    charControllers.push_back(newCtrl);

    newCtrl->name = "PhysX Character Controller";
    newCtrl->tag = "Character Controller";
    newCtrl->id = gameObjectID++;

    if (playerController->getActor()) {
        auto ptr = static_cast<GameObject*>(newCtrl);
        playerController->getActor()->userData = ptr;

        // FIX: init charctrl's fmod handle and other data
        std::string groupName = "ObjGroup_" + std::to_string(gameObjectID);
        FMOD_RESULT result = system->createChannelGroup(groupName.c_str(), &ptr->channelGroup);
        ptr->channelGroup->setMode(FMOD_2D);
        ptr->engPtr = this;

        ptr->physicsActor = playerController->getActor();
    }

	return newCtrl;
}

void CharacterController::Move(Vector3 direction, float speed, float dt) {
    if (!playerController) return;

    glm::vec3 horizontalDir = glm::vec3(direction.x, direction.y, 0.0f);

    if (glm::length(horizontalDir) > 0.0001f) {
        horizontalDir = glm::normalize(horizontalDir);
    }

    glm::vec3 displacement = horizontalDir * speed * dt;

    verticalVelocity += gravity * dt;
    displacement.z += verticalVelocity * dt;

    PxControllerFilters filters;
    PxControllerCollisionFlags flags = playerController->move(
        { displacement.x, displacement.y, displacement.z },
        0.001f, // minMoveDistance
        dt,
        filters
    );

    if (flags & PxControllerCollisionFlag::eCOLLISION_DOWN) {
        verticalVelocity = -0.1f; // Small constant downward force to stay glued to slopes
    }
}

void CharacterController::Jump(float force) {
    PxControllerState state;
    playerController->getState(state);

    if (state.collisionFlags & PxControllerCollisionFlag::eCOLLISION_DOWN) {
        verticalVelocity = force;
    }
}

Vector3 CharacterController::getPosition() {
    if (!playerController) return { 0.0f, 0.0f, 0.0f };

    // PhysX returns PxExtendedVec3 (doubles)
    PxExtendedVec3 physPos = playerController->getPosition();

    // Cast to your engine's Vector3 (floats)
    return {
        static_cast<float>(physPos.x),
        static_cast<float>(physPos.y),
        static_cast<float>(physPos.z)
    };
}

void CharacterController::setPosition(Vector3 position) {
    playerController->setPosition(PxExtendedVec3(position.x, position.y, position.z));
    verticalVelocity = 0.0f;
}

float CharacterController::getVerticalVelocity() {
    return verticalVelocity;
}

void Engine::requestDestroyCharacterController(ICharacterController* ctrl)
{
    characterControllerDestroyQueue.push(ctrl);
}

void Engine::checkCharacterControllerDestroy() {
    while (!characterControllerDestroyQueue.empty()) {
        auto& rawCtrl = characterControllerDestroyQueue.front();
        auto ctrl = static_cast<CharacterController*>(rawCtrl);

        if (!ctrl) return;

        // 1. release PhysX object
        if (ctrl->playerController)
        {
            ctrl->playerController->release();
            ctrl->playerController = nullptr;
        }

        // 2. remove from list
        auto it = std::find(charControllers.begin(), charControllers.end(), ctrl);
        if (it != charControllers.end())
            charControllers.erase(it);

        // 3. free wrapper
        delete ctrl;

        characterControllerDestroyQueue.pop();
    }
}