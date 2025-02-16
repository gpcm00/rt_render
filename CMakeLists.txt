cmake_minimum_required(VERSION 3.22)
project(vkpt VERSION 1.0)

find_package(Vulkan REQUIRED)


add_executable(renderer src/main.cpp)
target_link_libraries(renderer Vulkan::Vulkan)
target_include_directories(renderer PRIVATE include)
