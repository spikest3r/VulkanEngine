#include <engine.h>
#include <iostream>

class Scene2 : public Scene {
protected:
	void InitScene(Engine* engine) override;
};

void Scene2::InitScene(Engine* engine) {
	std::cout << "scene2" << std::endl;
}

class Scene3 : public Scene2 {
	void InitScene(Engine* engine) override;
};

void Scene3::InitScene(Engine* engine) {
	Scene2::InitScene(engine);
	std::cout << "scene3" << std::endl;
}

int main() {
	Engine* engine = Engine::Create();
	engine->init(800, 600, "SceneLoadTest");

	engine->setClearColor({ 0.2f,0.2f,0.2f });

	Scene* scene = engine->createScene<Scene3>("D:\\engine\\VulkanEngine-65b374bc2e12a985c3385b342ba707b8312c78aa\\Editor\\textscene.txt", nullptr);
	engine->loadScene(scene);

	engine->cameraPosition = Vector3(0, -15, 2);
	engine->cameraRotation = Vector3(0, 90, 0);

	while (engine->running()) {
		engine->update();
		engine->updateScene();
		engine->render();
	}

	engine->cleanup();
	Engine::Destroy(engine);

	return 0;
}