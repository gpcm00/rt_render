cmake_minimum_required(VERSION 3.22)
project(vkpt VERSION 1.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Dependencies
find_package(Vulkan REQUIRED)
add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/tinygltf)
# include_directories(external/stb)
add_subdirectory(external/VulkanMemoryAllocator)

add_executable(renderer 
src/main.cpp 
src/renderer.cpp 
src/input_system.cpp 
src/swapchain.cpp 
src/geometry.cpp
src/pipeline.cpp
)
target_link_libraries(renderer Vulkan::Vulkan glfw glm tinygltf GPUOpen::VulkanMemoryAllocator)
target_include_directories(renderer PRIVATE include)

file(GLOB shaders_sources 
shaders/*.vert 
shaders/*.frag 
shaders/*.comp 
shaders/*.geom 
shaders/*.tesc 
shaders/*.tese 
shaders/*.rgen 
shaders/*.rint
shaders/*.rahit 
shaders/*.rchit 
shaders/*.rmiss 
shaders/*.rcall 
)

foreach(shader ${shaders_sources})
    file(RELATIVE_PATH shader_rel ${CMAKE_CURRENT_SOURCE_DIR} ${shader})
    set(shader_output ${CMAKE_BINARY_DIR}/${shader_rel}.spv)
    add_custom_command(
        OUTPUT ${shader_output}
        COMMAND glslangValidator --target-env vulkan1.3 -V ${shader} -o ${shader_output}
        DEPENDS ${shader}
    )
    list(APPEND shader_outputs ${shader_output})
endforeach()

add_custom_target(shaders ALL DEPENDS ${shader_outputs})
add_dependencies(renderer shaders)

# Copy shader outputs to the renderer build output folder
add_custom_command(
    TARGET renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:renderer>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/shaders $<TARGET_FILE_DIR:renderer>/shaders
)

