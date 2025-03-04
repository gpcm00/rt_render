cmake_minimum_required(VERSION 3.22)
project(vkpt VERSION 1.0)

# Dependencies
find_package(Vulkan REQUIRED)
add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/tinygltf)


add_executable(renderer src/main.cpp src/input_system.cpp src/swapchain.cpp src/geometry.cpp)
target_link_libraries(renderer Vulkan::Vulkan glfw glm tinygltf)
target_include_directories(renderer PRIVATE include)
