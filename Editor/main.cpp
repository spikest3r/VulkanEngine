#include <engine.h>
#include <engine_ui.h>
#include <iostream>
#include <cmath>
#include <numbers>
#include <deque>
#include <map>
#include <fstream>

constexpr double DEG2RAD = std::numbers::pi / 180.0;

static Vector3 cameraPos;

struct FPSCamera {
	float moveSpeed = 10.0f;
	float mouseSensitivity = 0.1f;
	float pitch = 0.0f;
	float yaw = -90.0f;
	float height = 2.5f;
	bool firstMouse = true;
	float lastX = 0.0f;
	float lastY = 0.0f;
};

void UpdateFPSCamera(FPSCamera& cam, Engine* engine) {
	float dt = engine->getDeltaTime();
	float velocity = cam.moveSpeed * dt;

	// Mouse look
	Vector2 mouse = engine->getMousePos();

	if (cam.firstMouse) {
		cam.lastX = mouse.x;
		cam.lastY = mouse.y;
		if (cam.lastX != 0.0f || cam.lastY != 0.0f)
			cam.firstMouse = false;
	}

	if (!cam.firstMouse) {
		float dx = cam.lastX - mouse.x;
		float dy = cam.lastY - mouse.y;
		cam.lastX = mouse.x;
		cam.lastY = mouse.y;

		cam.yaw += dx * cam.mouseSensitivity;
		cam.pitch += dy * cam.mouseSensitivity;

		if (cam.pitch > 89.0f)  cam.pitch = 89.0f;
		if (cam.pitch < -89.0f) cam.pitch = -89.0f;

		engine->cameraRotation.x = cam.pitch;
		engine->cameraRotation.y = cam.yaw;
	}

	// Movement
	Vector3 forward, right;
	engine->getCameraVectors(forward, right);

	if (engine->getKey(KeyCode::W) == PRESS) engine->cameraPosition += forward * velocity;
	if (engine->getKey(KeyCode::S) == PRESS) engine->cameraPosition -= forward * velocity;
	if (engine->getKey(KeyCode::A) == PRESS) engine->cameraPosition -= right * velocity;
	if (engine->getKey(KeyCode::D) == PRESS) engine->cameraPosition += right * velocity;

	if (engine->getKey(KeyCode::E) == PRESS) cam.height += velocity;
	if (engine->getKey(KeyCode::C) == PRESS) cam.height -= velocity;

	engine->cameraPosition.z = cam.height;
	cameraPos = engine->cameraPosition;
}

static bool sceneImportButton = false;
static std::string g_sceneFile;
static bool sceneFileOpened = false;

PhysicsMaterial* material;

struct SceneResource {
	std::string name;
	std::string fileName;

	int id = 0;

	bool operator==(const SceneResource& other) const
	{
		return id == other.id;
	}
};

struct SceneGameObject {
	int id = 0;

	Transform transform;
	std::string meshName;
	std::string textureName;
	char gameObjectType[512] = "";
	bool dynamic;
	char gameObjectName[512] = "";
	char gameObjectTag[128] = "";

	bool operator==(const SceneGameObject& other) const
	{
		return id == other.id;
	}
};

struct EditorGameObject {
	int id = 0;

	int sgoId = 0;
	GameObject* go;

	bool operator==(const EditorGameObject& other) const
	{
		return id == other.id;
	}
};

class EditorScene : public Scene {
public:
	char* getSceneFileName() {
		return sceneFileName;
	}

	std::vector<SceneResource> meshes;
	std::vector<SceneResource> textures;

	std::map<int, SceneGameObject> gameObjectsMap;

	std::deque<GameObject*> runtimeGameObjects;
	std::deque<EditorGameObject> objectList;
private:
	void InitScene(Engine* engine) override;
	void UpdateScene(Engine* engine) override;
	GameObject* CreateGameObject(Engine* engine, const char* objectType, const char* tag, const char* name, Transform transform, Mesh* mesh, Texture* texture, bool dynamic) override;
	void ResourceLoaded(std::string name, const char* path, ResourceType type) override;

	void EditorUI();

	int globalId = 0;

	bool importMeshButton;
	bool importTextureButton;
	bool createGameObjectButton;
	bool destroyGameObjectButton;
	bool sceneExportButton;
	bool texUpd;
	bool resetCam;

	bool resourcePane = false;
	bool objectListPane = false;
	bool sceneMgmtPane = false;
	bool objectCreatePane = false;
	bool cameraInfoPane = false;

	char name[128];
	char path[128];

	char nearPlaneField[32] = "0.1";
	char farPlaneField[32] = "100.0";
	char moveSpeedField[32] = "10.0";

	char objectName[128];
	char objectType[128];
	Transform objectTransform;
	Vector3 objectRotation; // euler
	char objectMesh[128];
	char objectTexture[128];
	char objectTexture2[128];

	char sceneFileName[128];

	bool gameObjectSelected;
	EditorGameObject* seletcedGameObject;
	SceneGameObject* selectedSGO;

	void ExportScene(const char*);

	FPSCamera camera;
	bool gui = true;
	bool f12 = false;

	NearFarPlanes planes = { 0.1f, 100.0f };
	Vector3 clearColor = { 0.2f,0.2f,0.2f };
};

void EditorScene::ResourceLoaded(std::string name, const char* path, ResourceType type) {
	switch (type) {
	case MESH:
	{
		SceneResource res;
		res.name = name;
		res.fileName = path;
		res.id = globalId++;
		meshes.push_back(res);
		break;
	}
	case TEXTURE:
	{
		SceneResource res;
		res.name = name;
		res.fileName = path;
		res.id = globalId++;
		textures.push_back(res);
		break;
	}
	default:
	{
		std::cout << "Invalid mesh type!" << std::endl;
		break;
	}
	}
}

GameObject* EditorScene::CreateGameObject(Engine* engine, const char* objectType, const char* tag, const char* name, Transform transform, Mesh* mesh, Texture* texture, bool dynamic) {
	auto object = engine->createGameObject<GameObject>(transform, mesh, texture, material, dynamic);
	object->name = name;
	object->tag = tag;

	runtimeGameObjects.push_back(object);

	SceneGameObject gameObject;
	strcpy_s(gameObject.gameObjectName, 512, name);
	strcpy_s(gameObject.gameObjectType, 512, objectType);
	strcpy_s(gameObject.gameObjectTag, 128, tag);
	gameObject.meshName = mesh->getName();
	gameObject.textureName = texture->getName();
	gameObject.dynamic = false;
	gameObject.transform = objectTransform;
	gameObject.id = globalId++;

	int sgoId = gameObject.id;
	gameObjectsMap[sgoId] = gameObject;

	EditorGameObject ego;
	ego.go = object;
	ego.sgoId = sgoId;
	ego.id = globalId++;
	objectList.push_back(ego);
	
	return object;
}

void EditorScene::InitScene(Engine* engine) {
	importMeshButton = false;

	engine->SetUICallback([this](Engine*) {EditorUI(); });
}

Quaternion EulerDegreesToQuaternion(Vector3 e)
{
	Vector3 r = {
		e.x * DEG2RAD,
		e.y * DEG2RAD,
		e.z * DEG2RAD
	};

	float cx = cosf(r.x * 0.5f);
	float sx = sinf(r.x * 0.5f);
	float cy = cosf(r.y * 0.5f);
	float sy = sinf(r.y * 0.5f);
	float cz = cosf(r.z * 0.5f);
	float sz = sinf(r.z * 0.5f);

	Quaternion q;
	q.w = cx * cy * cz + sx * sy * sz;
	q.x = sx * cy * cz - cx * sy * sz;
	q.y = cx * sy * cz + sx * cy * sz;
	q.z = cx * cy * sz - sx * sy * cz;

	return q;
}

Vector3 QuaternionToEulerDegrees(Quaternion q)
{
	Vector3 euler;

	// Roll (X)
	float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
	float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
	euler.x = atan2f(sinr_cosp, cosr_cosp) / DEG2RAD;

	// Pitch (Y)
	float sinp = 2.0f * (q.w * q.y - q.z * q.x);
	if (fabsf(sinp) >= 1.0f)
		euler.y = copysignf(90.0f, sinp); // clamp at gimbal lock
	else
		euler.y = asinf(sinp) / DEG2RAD;

	// Yaw (Z)
	float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
	float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
	euler.z = atan2f(siny_cosp, cosy_cosp) / DEG2RAD;

	return euler;
}

void EditorScene::EditorUI() {
	if (cameraInfoPane) { // keep even with hidden gui
		UI::Begin("Camera");
		if (UI::Button("Close")) cameraInfoPane = false;
		char posBuffer[128];
		sprintf_s(posBuffer, 128, "X: %f | Y: %f | Z: %f", cameraPos.x, cameraPos.y, cameraPos.z);
		UI::Text(posBuffer);
		char rotBuffer[128];
		sprintf_s(rotBuffer, 128, "Pitch: %f | Yaw: %f", camera.pitch, camera.yaw);
		UI::Text(rotBuffer);

		if (UI::Button("Reset transform")) {
			camera.pitch = 0.0f;
			camera.yaw = -90.0f;
			resetCam = true;
		}

		UI::InputFloat3("Clear color", clearColor);
		UI::Text("Clipping planes");
		if (UI::TextField("Near plane", nearPlaneField, 32, true)) {
			char* end;
			float f = std::strtof(nearPlaneField, &end);
			if (nearPlaneField == end) f = 0;
			planes.near = f;
		}
		if(UI::TextField("Far plane", farPlaneField, 32, true)) {
			char* end;
			float f = std::strtof(farPlaneField, &end);
			if (farPlaneField == end) f = 0;
			planes.far = f;
		}
		if (UI::TextField("Move speed", moveSpeedField, 32, true)) {
			char* end;
			float f = std::strtof(moveSpeedField, &end);
			if (moveSpeedField == end) f = 0;
			camera.moveSpeed = f;
		}
		UI::End();
	}

	if (!gui) return;

	UI::Begin("Toolbox");
	if (UI::Button("Scene settings")) sceneMgmtPane = !sceneMgmtPane;
	if (UI::Button("Resources")) resourcePane = !resourcePane;
	if (UI::Button("Create object")) objectCreatePane = !objectCreatePane;
	if (UI::Button("Object list")) objectListPane = !objectListPane;
	if (UI::Button("Camera info")) cameraInfoPane = !cameraInfoPane;
	UI::Text("Q - Toggle camera mode");
	UI::End();

	if (sceneMgmtPane) {
		UI::Begin("Scene settings");
		UI::Text("Import/export scene");
		UI::TextField("Scene file path", sceneFileName, 128, true);
		sceneExportButton = UI::Button("Export");
		sceneImportButton = UI::Button("Import");
		if (sceneFileOpened) {
			char buffer[512];
			sprintf_s(buffer, 512, "Scene file: %s", g_sceneFile);
			UI::Text(buffer);
			if (UI::Button("Save")) {
				sceneExportButton = true;
			}
		}
		UI::End();
	}

	if (resourcePane) {
		UI::Begin("Resources");
		UI::TextField("Resource Name", name, 128, true);
		UI::TextField("Resource Path", path, 128, true);
		importMeshButton = UI::Button("Import mesh");
		importTextureButton = UI::Button("Import texture");
		UI::End();
	}

	if (objectCreatePane) {
		UI::Begin("Game object creation");
		UI::TextField("Name", objectName, 128, true);
		UI::TextField("Type", objectType, 128, true);
		UI::InputFloat3("Position", objectTransform.position);
		UI::InputFloat3("Rotation", objectRotation);
		UI::InputFloat3("Scale", objectTransform.scale);
		UI::TextField("Mesh", objectMesh, 128, true);
		UI::TextField("Texture", objectTexture, 128, true);
		createGameObjectButton = UI::Button("Instantiate");
		UI::SameLine();
		if (UI::Button("Clear fields")) {
			memset(objectName, 0, 128);
			memset(objectType, 0, 128);
			memset(objectMesh, 0, 128);
			memset(objectTexture, 0, 128);

			objectTransform = { {0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f,1.0f}, {0.0f,0.0f,0.0f} };
			objectRotation = { 0.0f,0.0f,0.0f };
		}
		UI::End();
	}

	if (objectListPane) {
		UI::Begin("All game objects");
		int i = 1;
		for (auto& object : objectList) {
			auto it = gameObjectsMap.find(object.sgoId);
			const char* displayName = (it != gameObjectsMap.end()) ? it->second.gameObjectName : "???";

			char buffer[256];
			sprintf_s(buffer, 256, "%i. %s", i, displayName);
			if (UI::Button(buffer)) {
				seletcedGameObject = &object;
				selectedSGO = &gameObjectsMap[object.sgoId];
				gameObjectSelected = true;
				strcpy_s(objectTexture2, 128, selectedSGO->textureName.c_str());
			}
			i++;
		}
		UI::End();
	}

	if (gameObjectSelected && seletcedGameObject) {
		auto it = gameObjectsMap.find(seletcedGameObject->sgoId);
		const char* objName = (it != gameObjectsMap.end()) ? it->second.gameObjectName : "???";

		char buffer[256];
		sprintf_s(buffer, 256, "%s Properties", objName);
		UI::Begin("Game Object Properties");
		UI::Text(buffer);
		bool close = false;
		if (UI::Button("Close")) {
			close = true;
		}

		UI::SameLine();

		if (UI::Button("Remove")) {
			gameObjectSelected = false;
			destroyGameObjectButton = true;
			//seletcedGameObject = nullptr;
		}

		if(UI::TextField("Name", selectedSGO->gameObjectName, 512, true))
			seletcedGameObject->go->name = selectedSGO->gameObjectName;
		UI::TextField("Tag", selectedSGO->gameObjectTag, 128, true);
		UI::TextField("Type", selectedSGO->gameObjectType, 512, true);

		UI::TextField("Texture", objectTexture2, 128, true);
		UI::SameLine();
		if (UI::Button("Update texture")) {
			texUpd = true;
		}

		UI::Text("Transform");

		Transform trans = seletcedGameObject->go->transform;
		Vector3 rot = QuaternionToEulerDegrees(trans.rotation);

		bool changed = false;
		changed |= UI::InputFloat3("Position", trans.position);
		changed |= UI::InputFloat3("Rotation", rot);
		changed |= UI::InputFloat3("Scale", trans.scale);

		if (changed)
		{
			trans.rotation = EulerDegreesToQuaternion(rot);
			seletcedGameObject->go->transform = trans;
			gameObjectsMap[seletcedGameObject->sgoId].transform = trans;
		}

		UI::End();

		if (close) {
			gameObjectSelected = false;
			seletcedGameObject = nullptr;
		}
	}
}

void EditorScene::UpdateScene(Engine* engine) {
	engine->setClearColor(clearColor);

	if (engine->getKey(KeyCode::Q) == PRESS) {
		if (!f12) {
			camera.firstMouse = true;
			f12 = true;
			gui = !gui;
			engine->setCursorMode(gui ? NORMAL : DISABLED);
		}
	}
	else {
		f12 = false;
	}

	if (resetCam) {
		engine->cameraPosition = Vector3(0, -15, 2);
		UpdateFPSCamera(camera, engine);
		resetCam = false;
	}

	engine->planes = planes;

	if (!gui) {
		UpdateFPSCamera(camera, engine);
	}

	if (importMeshButton) {
		importMeshButton = false;

		try {
			Mesh* mesh = engine->createMesh(name, path);
			if (!mesh) {
				throw;
			}

			SceneResource res;
			res.name = name;
			res.fileName = path;
			res.id = globalId++;
			meshes.push_back(res);

			memset(name, 0, 128);
			memset(path, 0, 128);
		}
		catch (std::exception) {
			std::cout << "Mesh import failed" << std::endl;
		}
	}

	if (importTextureButton) {
		importTextureButton = false;

		try {
			Texture* mesh = engine->createTexture(name, path);
			if (!mesh) {
				throw;
			}

			SceneResource res;
			res.name = name;
			res.fileName = path;
			res.id = globalId++;
			textures.push_back(res);

			memset(name, 0, 128);
			memset(path, 0, 128);
		}
		catch (std::exception) {
			std::cout << "Texture import failed" << std::endl;
		}
	}

	if (createGameObjectButton) {
		createGameObjectButton = false;

		try {
			Mesh* mesh = engine->getMesh(objectMesh);
			Texture* texture = engine->getTexture(objectTexture);
			objectTransform.rotation = EulerDegreesToQuaternion(objectRotation);
			GameObject* object = engine->createGameObject<GameObject>(objectTransform, mesh, texture, material, false);

			SceneGameObject gameObject;
			strcpy_s(gameObject.gameObjectName, 512, objectName);
			strcpy_s(gameObject.gameObjectType, 512, objectType);
			strcpy_s(gameObject.gameObjectTag, 128, "DefaultTag");
			gameObject.meshName = objectMesh;
			gameObject.textureName = objectTexture;
			gameObject.dynamic = false;
			gameObject.transform = objectTransform;
			gameObject.id = globalId++;

			int sgoId = gameObject.id;
			gameObjectsMap[sgoId] = gameObject;

			EditorGameObject ego;
			ego.go = object;
			ego.sgoId = sgoId;
			ego.id = globalId++;
			objectList.push_back(ego);
		}
		catch (std::exception) {
			std::cout << "Failed to create game object" << std::endl;
		}
	}

	if (destroyGameObjectButton) {
		destroyGameObjectButton = false;

		if (!seletcedGameObject)
			return;

		const int targetSgoId = seletcedGameObject->sgoId;
		const int targetEditorId = seletcedGameObject->id;
		const int targetGameId = seletcedGameObject->go->getID();

		engine->requestDestroyGameObject(seletcedGameObject->go);

		// --- SceneGameObject removal from map ---
		gameObjectsMap.erase(targetSgoId);

		// --- Runtime GameObject removal ---
		runtimeGameObjects.erase(
			std::remove_if(runtimeGameObjects.begin(), runtimeGameObjects.end(),
				[&](GameObject* go)
				{
					return go->getID() == targetGameId;
				}),
			runtimeGameObjects.end());

		// --- Editor list removal ---
		objectList.erase(
			std::remove_if(objectList.begin(), objectList.end(),
				[&](const EditorGameObject& obj)
				{
					return obj.id == targetEditorId;
				}),
			objectList.end());

		seletcedGameObject = nullptr;
		gameObjectSelected = false;
	}

	if (sceneExportButton) {
		sceneExportButton = false;
		if (strnlen_s(sceneFileName, 128) > 0)
			ExportScene(sceneFileName);
		else
			ExportScene(g_sceneFile.c_str());
		memset(sceneFileName, 0, 128);
	}

	if (texUpd && seletcedGameObject) {
		texUpd = false;

		try {
			Texture* tex = engine->getTexture(objectTexture2);
			seletcedGameObject->go->updateTexture(tex);
			selectedSGO->textureName = objectTexture2;
		}
		catch (std::exception) {
			std::cout << "Failed to update texture! Check texture name and try again" << std::endl;
		}
	}
}

void EditorScene::ExportScene(const char* filePath)
{
	std::ofstream file(filePath);
	if (!file.is_open())
	{
		std::cout << "Failed to open file for export: " << filePath << std::endl;
		return;
	}

	// 1. write meshes
	file << "#meshes" << std::endl;
	for (auto& mesh : meshes) {
		file << mesh.fileName << " " << mesh.name << std::endl;
	}

	// 2. write textures
	file << "#textures" << std::endl;
	for (auto& texture : textures) {
		file << texture.fileName << " " << texture.name << std::endl;
	}

	file << "#objects" << std::endl;
	for (auto& [id, sgo] : gameObjectsMap)
	{
		// type and name
		file << sgo.gameObjectType << " " << sgo.gameObjectName << " " << sgo.gameObjectTag << std::endl;
		
		// transform
		Transform trans = sgo.transform;
		file << "Pos " << trans.position.x << " " << trans.position.y << " " << trans.position.z;
		file << " Rot " << trans.rotation.x << " " << trans.rotation.y << " " << trans.rotation.z << " " << trans.rotation.w;
		file << " Scl " << trans.scale.x << " " << trans.scale.y << " " << trans.scale.z;
		file << std::endl;
		
		// properties
		file << "Mesh " << sgo.meshName << std::endl;
		file << "Texture " << sgo.textureName << std::endl;
		file << "Dynamic " << (sgo.dynamic ? "True" : "False") << std::endl;

		// end
		file << std::endl;
	}

	file.close();

	g_sceneFile = filePath;
	sceneFileOpened = true;
}

int main() {
	Engine* engine = Engine::Create();
	engine->init(800, 600, "VkEngine Editor");

	material = engine->createPhysicsMaterial(0.5f, 0.5f, 0.5f);

	EditorScene* scene = engine->createScene<EditorScene>();
	engine->loadScene(scene);

	engine->cameraPosition = Vector3(0, -15, 2);
	engine->cameraRotation = Vector3(0, 90, 0);
	
	bool stage1 = false;

	std::string sceneFile;

	while (engine->running()) {
		engine->update();

		if (sceneImportButton) {
			sceneImportButton = false;

			sceneFile = scene->getSceneFileName();

			engine->unloadActiveScene();
			engine->requestDestroyScene(scene);

			stage1 = true;
		} else if (stage1) {
			bool success;
			scene = engine->createScene<EditorScene>(sceneFile.c_str(), &success);
			engine->loadScene(scene);

			if (engine->getActiveScene() && success)
			{
				g_sceneFile = sceneFile.c_str();
				sceneFileOpened = true;
			}

			stage1 = false;
		}

		engine->updateScene();
		engine->render();
	}

	engine->cleanup();
	Engine::Destroy(engine);

	return 0;
}