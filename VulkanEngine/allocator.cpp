#include "engine.h"

#ifdef _WIN32
#include <malloc.h>   // _aligned_malloc, _aligned_free
#else
#include <cstdlib>
#include <cstring>
#endif

void* Engine::requestMemory(size_t size) {
    void* memory = nullptr;

#ifdef _WIN32
    memory = _aligned_malloc(size, 256);
    if (!memory) return nullptr;
#else
    if (posix_memalign(&memory, 256, size) != 0) {
        return nullptr;
    }
#endif

    std::memset(memory, 0, size);
    return memory;
}

void Engine::freeMemory(void* ptr) {
    if (!ptr) return;

#ifdef _WIN32
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}