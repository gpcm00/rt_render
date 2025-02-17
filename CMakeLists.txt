cmake_minimum_required(VERSION 3.22)
project(vkpt VERSION 1.0)

# Dependencies
find_package(Vulkan REQUIRED)
add_subdirectory(external/glfw)


add_executable(renderer src/main.cpp src/input_system.cpp)
target_link_libraries(renderer Vulkan::Vulkan Vulkan::Headers glfw)
target_include_directories(renderer PRIVATE include)
