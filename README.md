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

Configure the build directory and build the project:

```bash
cmake -B build
cmake --build build
```

Download the glTF sample assets:

```bash
cd build
git clone --depth 1 https://github.com/KhronosGroup/glTF-Sample-Assets
```

Run the application from the same directory as the glTF sample assets.
