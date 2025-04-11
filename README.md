# rt_render

A real-time path tracing renderer in Vulkan.

Originally an SFU CMPT 469/722 course project during the Spring 2025 term.

## Dependencies

Required:

- C++ 20 compiler
- CMake 3.22
- Vulkan 1.4 SDK

Included:

- GLFW
- OpenGL Mathematics (GLM)
- stb
- TinyGLTF
- Vulkan Memory Allocator (VMA)

## Building

Clone the repository with submodules:

```bash
git clone --recurse-submodules --shallow-submodules https://github.com/gpcm00/rt_render
```

Configure the build directory and build the project (Windows or Linux):

```bash
cmake -B build
cmake --build build
```

Download the glTF sample assets:

```bash
cd build
git clone --depth 1 https://github.com/KhronosGroup/glTF-Sample-Assets
```

Run the application from the same directory as the glTF sample assets. It uses the sample asset `ABeautifulGame.gltf` as the default scene, but you can pass it a file path as a command line argument, for example:

`./renderer.exe "glTF-Sample-Assets/Models/ABeautifulGame/glTF/ABeautifulGame.gltf"`

Due to time constraints, not all sample assets are supported. 

## Controls

- Movement: WASD keys
- Camera Rotation: Mouse
- Exit: Escape key

## Code

The code organization is as follows:

- The `shaders` folder contains the GLSL shader source code.
- Our implementation is split across the `src` folder which contains our `.cpp` files while the header files are in the `include` folder. Our renderer code is primarily in [`include/renderer/renderer.hpp`](include/renderer/renderer.hpp) and [`src/renderer.cpp`](src/renderer.cpp).
- The `external` directory contains third-party dependencies in the form of git submodules.

