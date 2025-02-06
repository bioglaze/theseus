UNAME := $(shell uname)
ARCH := $(shell uname -m)

ifeq ($(ARCH), aarch64)
SIMD := -DSIMD_NEON
else
ifeq ($(ARCH), arm64)
SIMD := -DSIMD_NEON
else
SIMD := -DSIMD_SSE3
endif
endif

ifeq ($(UNAME), Linux)
CC := clang++
CS := clang
DEFINES := -DAPI_METAL=0 -DAPI_VULKAN=1 -DVK_USE_PLATFORM_WAYLAND_KHR -D_DEBUG $(SIMD) -DSURFACE_EXTENSION_NAME=VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
LINKER := -ldl -lvulkan -lwayland-client -lwayland-cursor -ldecor-0
FLAGS := -I/usr/include/libdecor-0/ -std=c++17 -g -Wall -Wextra -pedantic -Wno-unused-function -Wno-unused-parameter -Wshadow -Wunreachable-code -Iinclude -Ithirdparty -Ivideo -Icore -Ithirdparty/imgui
else
CC := clang++
DEFINES := -DAPI_METAL=1 -DAPI_VULKAN=0 $(SIMD)
LINKER := -ldl
FLAGS := -std=c++17 -framework MetalKit -Iinclude -Ithirdparty -Ivideo -Icore -Ithirdparty/imgui
endif

SANITIZERS := -fsanitize=address,undefined
# SANITIZERS := -O3 -DNDEBUG

imgui:
	$(CC) -c -std=c++17 thirdparty/imgui/*.cpp

engine:
	mkdir -p ../build
ifeq ($(UNAME), Darwin)
	$(CC) $(DEFINES) $(SANITIZERS) -mmacos-version-min=14.7.2 -std=c++17 -g -Iinclude -Icore -Ivideo -Ithirdparty/metal_cpp -Ithirdparty/metal_ext -c unity.cpp -fno-objc-arc -o ../build/engine.o
	$(CC) $(DEFINES) $(SANITIZERS) -mmacos-version-min=14.7.2 -std=c++17 -g -Iinclude -Icore -Ivideo -Ithirdparty/imgui -Ithirdparty/metal_cpp -Ithirdparty/metal_ext -framework Cocoa -framework QuartzCore -framework MetalKit -framework Metal *.o ../build/engine.o samples/hello/include_metal.cpp samples/hello/hello_mac.mm -o ../build/hello_mac
	xcrun -sdk macosx metal shaders/metal/*.metal -o ../build/shaders.metallib
else
	$(CC) $(FLAGS) $(DEFINES) $(SANITIZERS) -c unity.cpp -o ../build/engine.o
	$(CC) $(FLAGS) $(DEFINES) $(SANITIZERS) samples/hello/hello.cpp *.o ../build/engine.o $(LINKER) -o ../build/hello
endif

toolz:
ifeq ($(UNAME), Darwin)
	$(CC) $(FLAGS) $(SANITIZERS) -mmacos-version-min=14.7.2 -std=c++17 -Iinclude -Ithirdparty/meshoptimizer -Ithirdparty/metal_cpp -Ithirdparty/metal_ext -framework Cocoa -framework Metal -framework MetalKit thirdparty/meshoptimizer/*.cpp tools/convert_obj/convert_obj.cpp -fno-objc-arc $(LINKER) -o ../build/convert_obj
	$(CC) $(DEFINES) $(SANITIZERS) -mmacos-version-min=14.7.2 -std=c++17 -g -Iinclude -Icore -Ivideo -Ithirdparty/imgui -Ithirdparty/metal_cpp -Ithirdparty/metal_ext -framework Cocoa -framework QuartzCore -framework MetalKit -framework Metal *.o ../build/engine.o samples/hello/include_metal.cpp tools/editor/sceneview.cpp tools/editor/main_mac.mm -o ../build/editor_mac
else
	$(CC) $(FLAGS) $(SANITIZERS) -std=c++17 -Iinclude -Ithirdparty/meshoptimizer thirdparty/meshoptimizer/*.cpp tools/convert_obj/convert_obj.cpp -o ../build/convert_obj
	$(CC) $(FLAGS) $(SANITIZERS) tools/editor/*.cpp *.o ../build/engine.o $(LINKER) -o ../build/editor
endif
