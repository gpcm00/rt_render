LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
STB_INCLUDE_PATH = stb
TINYOBJ_INCLUDE_PATH = tinyobjloader
TINYGLTFINCLUDE_PATH = ../../external/tinygltf

CFLAGS = -std=c++17 -O2 -I$(STB_INCLUDE_PATH) -I$(TINYOBJ_INCLUDE_PATH) -I$(TINYGLTFINCLUDE_PATH)


VulkanTest: scene_sample.cpp
	g++ $(CFLAGS) -o VulkanTest scene_sample.cpp geometry.cpp $(LDFLAGS) -g

Geometry: geometry.cpp
	g++ $(CFLAGS) -o geometry geometry.cpp $(LDFLAGS) -g
	
.PHONY: test clean

test: VulkanTest
	./VulkanTest

clean:
	rm -f VulkanTest