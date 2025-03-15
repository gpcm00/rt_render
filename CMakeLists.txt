cmake_minimum_required(VERSION 3.22)
project(vkpt VERSION 1.0)

# Dependencies
find_package(Vulkan REQUIRED)
add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/tinygltf)

# CUDA and Optix
find_package(CUDA REQUIRED)
find_package(CUDAToolkit REQUIRED)
include(cmake/FindOptiX90.cmake)
# set(OptiX_INSTALL_DIR "C:/ProgramData/NVIDIA Corporation/OptiX SDK 9.0.0/SDK")

# Refer to https://github.com/NVIDIA/OptiX_Apps
if(OptiX90_FOUND)
  set(OPTIX_INCLUDE_DIR "${OPTIX90_INCLUDE_DIR}")
  message(STATUS "Found OptiX SDK 9.0.0 at ${OPTIX90_PATH}")
else()
  message(FATAL_ERROR "No OptiX SDK 7/8/9 found.")
endif()
# find_package(OptiX REQUIRED)

add_executable(renderer src/main.cpp src/input_system.cpp src/swapchain.cpp src/geometry.cpp src/denoiser.cpp)
target_link_libraries(renderer Vulkan::Vulkan glfw glm tinygltf CUDA::cudart CUDA::cuda_driver)
target_include_directories(renderer PRIVATE include ${OPTIX_INCLUDE_DIR})

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
        COMMAND glslangValidator -V ${shader} -o ${shader_output}
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

