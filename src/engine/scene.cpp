#include "engine.h"
#include "scene.h"

#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

float ParseFloat(const std::string& s, size_t lineNumber) {
    try {
        size_t idx;
        float value = std::stof(s, &idx);

        // check if entire string was consumed
        if (idx != s.size()) {
            std::cerr << "Parse error line " << lineNumber
                << ": invalid float \"" << s << "\"\n";
            return 0.0f;
        }

        return value;
    }
    catch (const std::exception&) {
        std::cerr << "Parse error line " << lineNumber
            << ": failed to parse float \"" << s << "\"\n";
        return 0.0f;
    }
}

// --- utility: split line by spaces ---
static std::vector<std::string> Split(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;

    while (iss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

void Scene::EarlyInitScene(Engine* engine) {

}

void Scene::InitScene(Engine* engine) {
	// base scene initialized
}

void Scene::UpdateScene(Engine* engine) {

}

void Scene::DestroyScene(Engine* engine) {

}

void Scene::ResourceLoaded(std::string name, const char* path, ResourceType type) {

}

GameObject* Scene::CreateGameObject(Engine* engine, const char* objectType, const char* tag, const char* name, Transform transform, Mesh* mesh, Texture* texture, bool dynamic) {
	// user handles all their custom types here
    auto object = engine->createGameObject<GameObject>(transform, mesh, texture, engine->getDefaultMaterial(), dynamic);
    object->name = name;
    object->tag = tag;
    return object;
}

enum ParseSection {
    NonePS = 0,
    Meshes,
    Textures,
    GameObjects
};

enum TransformFieldSection {
    Pos,
    Rot,
    Scl,
    NoneTFS = 999
};

struct TokenCount {
    static constexpr int Header = 2;
    static constexpr int Transform = 14;
    static constexpr int Property = 2;
};

void ReportError(int line, const char* error) {
    std::cout << "Error on line " << line << ": " << error << std::endl;
}

bool Engine::loadScene_internal(Scene* scene, const char* sceneFile)
{
    std::ifstream file(sceneFile);

    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << sceneFile << "\n";

        char errorBuffer[1024];
        snprintf(errorBuffer, 1024, "Failed to load scene %s", sceneFile);
        OnError_Handler(errorBuffer);

        return false;
    }

    std::string line;
    size_t lineNumber = 0;

    ParseSection section = NonePS;

    SceneGameObject temp{};
    bool parsingObject = false;

    auto finishObject = [&]() {
        if (parsingObject) {
            scene->sceneGameObjects.push_back(temp);
            temp = SceneGameObject{};
            parsingObject = false;
        }
        };

    while (std::getline(file, line))
    {
        lineNumber++;

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty()) {
            if (section == GameObjects) finishObject();
            continue;
        }

        // ---------------- SECTION SWITCH ----------------
        if (line[0] == '#')
        {
            finishObject();

            if (line == "#meshes") section = Meshes;
            else if (line == "#textures") section = Textures;
            else if (line == "#objects") section = GameObjects;
            else section = NonePS;

            continue;
        }

        std::vector<std::string> tokens = Split(line);

        switch (section)
        {
        case Meshes:
        {
            if (tokens.size() != 2) {
                ReportError(lineNumber, "Meshes: expected 2 tokens");
                return false;
            }

            scene->sceneMeshes.emplace_back(
                std::string(tokens[1]),
                std::string(tokens[0])
            );

            scene->ResourceLoaded(tokens[1], tokens[0].c_str(), ResourceType::MESH);

            break;
        }

        case Textures:
        {
            if (tokens.size() != 2) {
                ReportError(lineNumber, "Textures: expected 2 tokens");
                return false;
            }

            scene->sceneTextures.emplace_back(
                std::string(tokens[1]),
                std::string(tokens[0])
            );

            scene->ResourceLoaded(tokens[1], tokens[0].c_str(), ResourceType::TEXTURE);

            break;
        }

        case GameObjects:
        {
            // ---------------- OBJECT HEADER ----------------
            if (!parsingObject)
            {   
                if (tokens.size() != 3) {
                    ReportError(lineNumber, "GameObject header must be: Type Name Tag");
                    return false;
                }

                temp = SceneGameObject{};
                temp.gameObjectType = tokens[0];
                temp.gameObjectName = tokens[1];
                temp.gameObjectTag = tokens[2];
                parsingObject = true;
                continue;
            }

            // ---------------- TRANSFORM LINE ----------------
            if (tokens.size() >= 10)
            {
                enum { NONE, POS, ROT, SCL } mode = NONE;
                int idx = 0;

                auto apply = [&](const std::string& t) {
                    if (t == "Pos") { mode = POS; idx = 0; return; }
                    if (t == "Rot") { mode = ROT; idx = 0; return; }
                    if (t == "Scl") { mode = SCL; idx = 0; return; }

                    float v = ParseFloat(t, lineNumber);

                    switch (mode)
                    {
                    case POS:
                        (&temp.transform.position.x)[idx++] = v;
                        break;
                    case ROT:
                        (&temp.transform.rotation.x)[idx++] = v;
                        break;
                    case SCL:
                        (&temp.transform.scale.x)[idx++] = v;
                        break;
                    default:
                        ReportError(lineNumber, "Transform missing Pos/Rot/Scl");
                        break;
                    }
                    };

                for (const auto& t : tokens)
                    apply(t);

                continue;
            }

            // ---------------- PROPERTIES ----------------
            if (tokens.size() == 2)
            {
                const auto& key = tokens[0];
                const auto& value = tokens[1];

                if (key == "Mesh") temp.meshName = value;
                else if (key == "Texture") temp.textureName = value;
                else if (key == "Dynamic") temp.dynamic = (value == "True");
                else {
                    ReportError(lineNumber, "Unknown property");
                }
            }

            break;
        }

        default:
            break;
        }
    }

    finishObject();

    scene->sceneFileName = std::string(sceneFile);
    sceneMap[sceneFile] = scene;

    return true;
}

void Engine::cleanupState() {
    for (auto& object : gameObjects) {
        requestDestroyGameObject(object);
    }

    for (auto& texture : textures) {
        requestDestroy(texture);
    }

    for (auto& sound : sounds) {
        requestDestroy(sound);
    }

    for (auto& mesh : meshes) {
        if (mesh->engineMember) continue;
        requestDestroy(mesh);
    }

    for (auto& element : uiElements) {
        // TODO: INTO QUEUE
        // THIS IS NOT READY FOR A PROD!
        uiElements.pop_back();
    }
}

void Engine::loadScene(Scene* scene) {
    // cleanup everything
    if(activeScene) unloadActiveScene();

    forceDestroy();

    scene->EarlyInitScene(this);

    for (auto& mesh : scene->sceneMeshes) {
        createMesh(mesh.name, mesh.fileName.c_str());
    }

    for (auto& texture : scene->sceneTextures) {
        createTexture(texture.name, texture.fileName.c_str());
    }

    for (auto& gameObject : scene->sceneGameObjects) {
        Mesh* mesh = getMesh(gameObject.meshName);
        Texture* texture = getTexture(gameObject.textureName);

        scene->CreateGameObject(this,
            gameObject.gameObjectType.c_str(),
            gameObject.gameObjectTag.c_str(),
            gameObject.gameObjectName.c_str(),
            gameObject.transform,
            mesh,
            texture,
            gameObject.dynamic);
    }

    scene->InitScene(this);
    activeScene = scene;
}

void Engine::unloadActiveScene() {
    if (activeScene) {
        vkDeviceWaitIdle(device);
        activeScene->DestroyScene(this);
    }

    cleanupState();
    activeScene = nullptr;
}

Scene* Engine::getActiveScene() {
    return activeScene;
}

void Engine::updateScene() {
    if (!activeScene) return;
    activeScene->UpdateScene(this);
}

void Engine::requestDestroyScene(Scene* scene) {
    sceneDestroyQueue.push(scene);
}

void Engine::checkSceneDestroy() {
    while (!sceneDestroyQueue.empty()) {
        auto& scene = sceneDestroyQueue.front();

        if (!scene) return;

        sceneMap.erase(scene->sceneFileName);

        // 3. free wrapper
        ObjectHeader* h = getHeader(scene);
        h->destroy(scene);

        sceneDestroyQueue.pop();
    }
}

Scene* Engine::getScene(std::string sceneFile) {
    auto it = sceneMap.find(sceneFile);
    if (it != sceneMap.end()) {
        return it->second;
    }
    return nullptr;
}