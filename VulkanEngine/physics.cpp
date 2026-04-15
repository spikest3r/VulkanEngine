#include "engine.h"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "cooking/PxSdfDesc.h"

Vector3 PxQuatToEuler(const physx::PxQuat& q) {
    // 1. Convert PhysX Quat to GLM Quat
    glm::quat gq(q.w, q.x, q.y, q.z);

    // 2. Convert to Euler Angles (returns radians)
    glm::vec3 euler = glm::eulerAngles(gq);

    // 3. Convert to Degrees (if your engine uses degrees)
    return {
        glm::degrees(euler.x),
        glm::degrees(euler.y),
        glm::degrees(euler.z)
    };
}

// collisions and triggers

void PhysicsEventListener::onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) {
    for (PxU32 i = 0; i < nbPairs; i++) {
        const PxContactPair& cp = pairs[i];

        if (cp.events & PxPairFlag::eNOTIFY_TOUCH_FOUND) {

            // 1. Retrieve our GameObject pointers
            GameObject* objA = static_cast<GameObject*>(pairHeader.actors[0]->userData);
            GameObject* objB = static_cast<GameObject*>(pairHeader.actors[1]->userData);

            // 2. Extract contact points to calculate Impulse
            PxVec3 totalImpulse(0.0f, 0.0f, 0.0f);

            // Create a small buffer to hold the contact points
            // 16 points is usually plenty for a standard collision
            PxContactPairPoint contactPoints[16];
            PxU32 nbContacts = cp.extractContacts(contactPoints, 16);

            for (PxU32 j = 0; j < nbContacts; j++) {
                totalImpulse += contactPoints[j].impulse;
            }

            float impulseMagnitude = totalImpulse.magnitude();

            // 3. Fire the game callbacks
            if (objA && objA->onCollision) objA->onCollision(objB, impulseMagnitude);
            if (objB && objB->onCollision) objB->onCollision(objA, impulseMagnitude);
        }
    }
}

void PhysicsEventListener::onTrigger(PxTriggerPair* pairs, PxU32 count) {
    for (PxU32 i = 0; i < count; i++) {
        // Use the base class pointer first to avoid casting issues
        Trigger* triggerBase = static_cast<Trigger*>(pairs[i].triggerActor->userData);
        GameObject* otherObj = static_cast<GameObject*>(pairs[i].otherActor->userData);

        if (!triggerBase) continue;


        if (pairs[i].status & PxPairFlag::eNOTIFY_TOUCH_FOUND) {
            if (triggerBase->onTriggerEnter) {
                triggerBase->onTriggerEnter(otherObj);
            }
        }
    }
}

PxFilterFlags EngineFilterShader(
    PxFilterObjectAttributes attributes0, PxFilterData filterData0,
    PxFilterObjectAttributes attributes1, PxFilterData filterData1,
    PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
    // Check if either object is a trigger
    if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1)) {
        // Triggers only need 'eTRIGGER_DEFAULT' (which includes touch found/lost)
        pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
        return PxFilterFlag::eDEFAULT;
    }

    pairFlags = PxPairFlag::eCONTACT_DEFAULT | PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_CONTACT_POINTS;
    return PxFilterFlag::eDEFAULT;
}

void Engine::initPhysX() {
    gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);

    PxTolerancesScale scale;
    gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, scale, true, nullptr);

    PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
    sceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS;
    sceneDesc.flags |= PxSceneFlag::eENABLE_CCD;
    sceneDesc.gravity = PxVec3(0.0f, 0.0f, -9.81f); // Change to (0,0,-9.81) for Z-Up

    PxDefaultCpuDispatcher* gDispatcher = PxDefaultCpuDispatcherCreate(2);
    sceneDesc.cpuDispatcher = gDispatcher;
    sceneDesc.filterShader = PxDefaultSimulationFilterShader;
    sceneDesc.filterShader = EngineFilterShader;
    sceneDesc.simulationEventCallback = mPhysicsEventListener;

    gScene = gPhysics->createScene(sceneDesc);

    // TODO: Floor?
    //PxMaterial* gMaterial = gPhysics->createMaterial(0.5f, 0.5f, 0.1f);
    //PxRigidStatic* groundPlane = PxCreatePlane(*gPhysics, PxPlane(0, 0, 1, 0), *gMaterial);
    //gScene->addActor(*groundPlane);

    // Character Controller
    gControllerManager = PxCreateControllerManager(*gScene);

    PxCookingParams params(gPhysics->getTolerancesScale());
    params.meshPreprocessParams |= PxMeshPreprocessingFlag::eWELD_VERTICES;
    params.meshWeldTolerance = 0.0001f;
#ifndef _WIN32
    mCooking = PxCreateCooking(PX_PHYSICS_VERSION, *gFoundation, params);
#endif

    gScene->setVisualizationParameter(PxVisualizationParameter::eSCALE, 1.0f);

    gScene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);

    gScene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_EDGES, 1.0f);
}

RaycastHit Engine::raycast(Vector3 origin, Vector3 direction, float distance) {
    RaycastHit result = { -1.0f, nullptr };

    // 1. Convert your custom Vector3 to PhysX types
    physx::PxVec3 pxOrigin(origin.x, origin.y, origin.z);
    physx::PxVec3 pxDir(direction.x, direction.y, direction.z);
    pxDir.normalize(); // Crucial for accurate PhysX raycasting

    // 2. Prepare the hit buffer
    physx::PxRaycastBuffer hit;

    // 3. Launch the raycast
    bool status = gScene->raycast(pxOrigin, pxDir, distance, hit);

    if (status && hit.hasBlock) {
        result.distance = hit.block.distance;

        // 4. Retrieve the actor and cast the userPtr back to GameObject
        physx::PxRigidActor* actor = hit.block.actor;
        if (actor && actor->userData) {
            result.object = static_cast<GameObject*>(actor->userData);
        }
    }

    return result;
}

PxRigidStatic* Engine::createStaticActor(Mesh* mesh, Vector3 scale, PxMaterial* material) {
    PxRigidStatic* staticActor = gPhysics->createRigidStatic(PxTransform(PxIdentity));

    PxTriangleMeshGeometry geometry(mesh->triMesh, PxMeshScale(PxVec3(scale.x, scale.y, scale.z)));

    PxShape* shape = gPhysics->createShape(geometry, *material);
    staticActor->attachShape(*shape);
    shape->release();

    gScene->addActor(*staticActor);
    return staticActor;
}

PxRigidDynamic* Engine::createDynamicActor(Mesh* mesh, Vector3 scale, PxMaterial* material) {
    PxRigidDynamic* dynamicActor = gPhysics->createRigidDynamic(PxTransform(PxIdentity));

    PxConvexMeshGeometry geometry(mesh->convexMesh, PxMeshScale(PxVec3(scale.x, scale.y, scale.z)));

    PxShape* shape = gPhysics->createShape(geometry, *material);

    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
    shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
    shape->setFlag(PxShapeFlag::eVISUALIZATION, true);

    dynamicActor->attachShape(*shape);
    shape->release();

    gScene->addActor(*dynamicActor);

    return dynamicActor;
}

glm::mat4 basis(
    1, 0, 0, 0,
    0, 0, 1, 0,
    0, 1, 0, 0,
    0, 0, 0, 1
);

void Engine::updatePhysics(float deltaTime) {
    // if (getKey(KeyCode::E) != PRESS) return;

    gScene->simulate(1.0f / 165.0f);
    gScene->fetchResults(true);

    PxU32 nbActiveActors;
    PxActor** activeActors = gScene->getActiveActors(nbActiveActors);

    for (PxU32 i = 0; i < nbActiveActors; ++i) {
        GameObject* obj = static_cast<GameObject*>(activeActors[i]->userData);

        if (obj && obj->isDynamic) {
            PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(activeActors[i]);
            PxTransform pose = dynamicActor->getGlobalPose();

            obj->transform.position = { pose.p.x, pose.p.y, pose.p.z };

            glm::quat physxQuat(pose.q.w, pose.q.x, pose.q.y, pose.q.z);

            glm::quat finalQuat = physxQuat;

            obj->transform.rotation = { finalQuat.x, finalQuat.y, finalQuat.z, finalQuat.w };
        }
    }
    int a = 0;
}

PhysicsMaterial* Engine::createPhysicsMaterial(float staticFriction, float dynamicFriction, float restitution) {
    materials.emplace_back();
    auto ptr = &materials.back();

    ptr->material = gPhysics->createMaterial(staticFriction, dynamicFriction, restitution);

    return ptr;
}

void Engine::requestDestroyTrigger(Trigger* trigger) {
    triggerDestroyQueue.push(trigger);
}

void Engine::checkTriggerDestroy() {
    while (!triggerDestroyQueue.empty()) {
        auto& res = triggerDestroyQueue.front();

        gScene->removeActor(*(res->physicsActor));
        res->physicsActor->release();

        std::function<void(GameObject* other)>().swap(res->onTriggerEnter);
        std::function<void(GameObject* other)>().swap(res->onTriggerExit);

        triggerDestroyQueue.pop();
    }
}

void Engine::cookMesh(Mesh* mesh) {

#ifdef _WIN32
    PxCookingParams params(gPhysics->getTolerancesScale());

    {
        PxTriangleMeshDesc meshDesc;
        std::vector<PxVec3> pxVerts;
        pxVerts.reserve(mesh->vertices.size());
        for (auto& v : mesh->vertices) {
            pxVerts.push_back(PxVec3(v.pos.x, v.pos.y, v.pos.z));
        }

        // Then use pxVerts for cooking:
        meshDesc.points.count = (PxU32)pxVerts.size();
        meshDesc.points.stride = sizeof(PxVec3);
        meshDesc.points.data = pxVerts.data();

        meshDesc.triangles.count = (PxU32)mesh->indices.size() / 3;
        meshDesc.triangles.stride = sizeof(uint32_t) * 3;
        meshDesc.triangles.data = mesh->indices.data();

        PxDefaultMemoryOutputStream writeBuffer;

        if (!PxCookTriangleMesh(params, meshDesc, writeBuffer))
            return;

        PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
        mesh->triMesh = gPhysics->createTriangleMesh(readBuffer);
    }

    {
        PxConvexMeshDesc convexDesc;
        convexDesc.points.count = (PxU32)mesh->vertices.size();
        convexDesc.points.stride = sizeof(Vertex);
        convexDesc.points.data = mesh->vertices.data();
        convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

        PxDefaultMemoryOutputStream writeBuffer;

        if (!PxCookConvexMesh(params, convexDesc, writeBuffer))
            return;

        PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
        mesh->convexMesh = gPhysics->createConvexMesh(readBuffer);
    }

#else
    {
        PxTriangleMeshDesc meshDesc;
        meshDesc.points.count = (PxU32)mesh->vertices.size();
        meshDesc.points.stride = sizeof(Vertex);
        meshDesc.points.data = mesh->vertices.data();

        meshDesc.triangles.count = (PxU32)mesh->indices.size() / 3;
        meshDesc.triangles.stride = sizeof(uint32_t) * 3;
        meshDesc.triangles.data = mesh->indices.data();

        PxDefaultMemoryOutputStream writeBuffer;

        if (!mCooking->cookTriangleMesh(meshDesc, writeBuffer))
            return;

        PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
        mesh->triMesh = gPhysics->createTriangleMesh(readBuffer);
    }

    {
        PxConvexMeshDesc convexDesc;
        convexDesc.points.count = (PxU32)mesh->vertices.size();
        convexDesc.points.stride = sizeof(Vertex);
        convexDesc.points.data = mesh->vertices.data();
        convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

        PxDefaultMemoryOutputStream writeBuffer;

        if (!mCooking->cookConvexMesh(convexDesc, writeBuffer))
            return;

        PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
        mesh->convexMesh = gPhysics->createConvexMesh(readBuffer);
    }

#endif
}

void Engine::renderPhysXDebug(bool state) {
    visualizePhysX = state;
}