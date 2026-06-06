cmake_minimum_required(VERSION 3.16)
project(VulkanEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DENGINE_EXPORTS)

# 1. Find System Dependencies
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(assimp REQUIRED)
find_package(Threads REQUIRED)

# 2. FMOD Configuration (Manual Path)
# Point this to where you extracted the fmod tarball
set(FMOD_ROOT ${CMAKE_SOURCE_DIR}/external/fmod)
set(FMOD_INC_DIR "${FMOD_ROOT}/inc")
set(FMOD_LIB_DIR "${FMOD_ROOT}/lib/x86_64")
set(FMOD_LIB "${FMOD_LIB_DIR}/libfmod.so")

# 3. PhysX Configuration
set(PHYSX_LIBS 
    PhysX
    PhysXCharacterKinematic_static
    PhysXCommon
    PhysXFoundation
    PhysXCooking
    PhysXExtensions_static
)

# 4. Collect Sources
file(GLOB_RECURSE SOURCES 
	src/*.cpp
)

# 5. Build the Shared Object (.so)
add_library(VulkanEngine SHARED ${SOURCES})

# 6. Include Paths
target_include_directories(VulkanEngine PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Vulkan_INCLUDE_DIRS}
    ${FMOD_INC_DIR}           # Added FMOD headers
    /usr/include/physx
    PRIVATE src/include/ 
)

# 7. Linkage
target_link_libraries(VulkanEngine PRIVATE 
    Vulkan::Vulkan
    glfw
    assimp
    ${FMOD_LIB}
    ${PHYSX_LIBS}
    Threads::Threads
)

if(UNIX AND NOT APPLE)
    target_link_libraries(VulkanEngine PRIVATE asound)
endif()

# Set output name to libVulkanEngine.so
set_target_properties(VulkanEngine PROPERTIES PREFIX "")

set(CMAKE_BUILD_TYPE Debug)
add_compile_definitions(_DEBUG)
