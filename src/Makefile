LINKER := -ldl -lvulkan -lwayland-client -lwayland-cursor -ldecor-0
FLAGS := -I/usr/include/libdecor-0/ -std=c++17 -Wall -Wextra -pedantic -Wno-unused-function -Wno-unused-parameter -Wshadow -Wunreachable-code -Iinclude -Ithirdparty -Ivideo -Icore -Ithirdparty/imgui
DEFINES := -DVK_USE_PLATFORM_WAYLAND_KHR -D_DEBUG -DSIMD_SSE3 -DSURFACE_EXTENSION_NAME=VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
CC := g++
CS := gcc
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

toolz:
	$(CC) $(FLAGS) $(SANITIZERS) -Ithirdparty/meshoptimizer thirdparty/meshoptimizer/*.cpp tools/convert_obj/convert_obj.cpp -o ../build/convert_obj

