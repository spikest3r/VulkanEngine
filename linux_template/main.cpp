#include "engine.h"

int main() {
	auto* a = Engine::Create();
	a->init(800, 600,"Hello");
	a->cleanup();
	Engine::Destroy(a);
	return 0;
}
