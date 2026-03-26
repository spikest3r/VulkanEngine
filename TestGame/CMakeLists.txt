cmake_minimum_required(VERSION 3.10)
project(TestGame)

# 1. C++ Standard (matching your vcxproj stdcpp20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. Add ASAN (as seen in your Debug/Release x64 config)
# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

# 3. Include Directories
# Points to where your engine headers are located
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    /home/oleh/engine/VulkanEngine-65b374bc2e12a985c3385b342ba707b8312c78aa/VulkanEngine
    /usr/include # Standard Arch location for GLFW/Vulkan
)

# 4. Library Search Paths
# Points to where your newly built VulkanEngine.so lives
link_directories(/home/oleh/engine/VulkanEngine-65b374bc2e12a985c3385b342ba707b8312c78aa/VulkanEngine/build)

# 5. Define the Executable
add_executable(TestGame 
    main.cpp
)

# 6. Linking
# You are correct: linking VulkanEngine (the .so) will resolve its 
# sub-dependencies (PhysX, FMOD, etc.) automatically at runtime.
target_link_libraries(TestGame 
    "/home/oleh/engine/VulkanEngine-65b374bc2e12a985c3385b342ba707b8312c78aa/VulkanEngine/build/VulkanEngine.so"
    glfw
)

# 7. Set RPATH so it finds the .so without needing LD_LIBRARY_PATH
set_target_properties(TestGame PROPERTIES 
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "/home/oleh/engine/VulkanEngine-65b374bc2e12a985c3385b342ba707b8312c78aa/build"
)
