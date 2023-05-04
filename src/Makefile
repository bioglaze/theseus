LINKER := -ldl -lvulkan -lwayland-client
FLAGS := -std=c++17 -Wall -Wextra -pedantic -Wno-unused-function -Wno-unused-parameter -Wshadow -Wunreachable-code -msse3 -Iinclude -Ithirdparty -Ivideo -Icore -Ithirdparty/imgui
DEFINES := -DVK_USE_PLATFORM_WAYLAND_KHR -D_DEBUG -DSIMD_SSE3 -DSURFACE_EXTENSION_NAME=VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
CC := clang++
CS := clang
else
CC := clang++
endif

SANITIZERS := -g -fsanitize=address,undefined
# SANITIZERS := -g -O3 -DNDEBUG
#SANITIZERS := -g -O3 -DNDEBUG

engine:
	mkdir -p ../build
	$(CC) $(FLAGS) $(DEFINES) $(SANITIZERS) -c unity.cpp -o ../build/engine.o
	$(CC) $(FLAGS) $(DEFINES) $(SANITIZERS) samples/hello/hello.cpp thirdparty/imgui/*.cpp ../build/engine.o $(LINKER) -o ../build/hello

