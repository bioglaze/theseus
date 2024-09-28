UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
CC := g++
CS := gcc
DEFINES := -DAPI_METAL=0 -DAPI_VULKAN=1 -DVK_USE_PLATFORM_WAYLAND_KHR -D_DEBUG -DSIMD_SSE3 -DSURFACE_EXTENSION_NAME=VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
LINKER := -ldl -lvulkan -lwayland-client -lwayland-cursor -ldecor-0
FLAGS := -I/usr/include/libdecor-0/ -std=c++17 -g -Wall -Wextra -pedantic -Wno-unused-function -Wno-unused-parameter -Wshadow -Wunreachable-code -Iinclude -Ithirdparty -Ivideo -Icore -Ithirdparty/imgui
else
CC := clang++
DEFINES := -DAPI_METAL=1 -DAPI_VULKAN=0 -DSIMD_NEON
LINKER := -ldl
FLAGS := -std=c++17 -g -Wall -Wextra -pedantic -Wno-unused-function -Wno-unused-parameter -Wshadow -Wunreachable-code -Iinclude -Ithirdparty -Ivideo -Icore -Ithirdparty/imgui
endif

SANITIZERS := -fsanitize=address,undefined
# SANITIZERS := -g -O3 -DNDEBUG
#SANITIZERS := -g -O3 -DNDEBUG

engine:
	mkdir -p ../build
ifeq ($(UNAME), Darwin)
	clang video/metal/*.mm video/textureloader.cpp core/math.cpp -Iinclude -Icore -Ivideo -std=gnu++20 -framework Metal -framework Foundation -lc++
else
	$(CC) $(FLAGS) $(DEFINES) $(SANITIZERS) -c unity.cpp -o ../build/engine.o
endif
	$(CC) $(FLAGS) $(DEFINES) $(SANITIZERS) samples/hello/hello.cpp thirdparty/imgui/*.cpp ../build/engine.o $(LINKER) -o ../build/hello

toolz:
	$(CC) $(FLAGS) $(SANITIZERS) -Ithirdparty/meshoptimizer thirdparty/meshoptimizer/*.cpp tools/convert_obj/convert_obj.cpp -o ../build/convert_obj
	$(CC) $(FLAGS) $(SANITIZERS) thirdparty/imgui/*.cpp tools/editor/*.cpp ../build/engine.o $(LINKER) -o ../build/editor
