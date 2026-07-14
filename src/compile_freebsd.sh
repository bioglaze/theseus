clang++ -DAPI_METAL=0 -DAPI_VULKAN=1 -DVK_USE_PLATFORM_WAYLAND_KHR -D_DEBUG -DSIMD_SSE3 -DSURFACE_EXTENSION_NAME=VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME -I/usr/include/libdecor-0/ -std=c++17 -g -Wall -Wextra -pedantic -Wno-unused-function -Wno-unused-parameter -Wshadow -Wunreachable-code -Iinclude -Ithirdparty -Ivideo -Icore -Ithirdparty/imgui -I/usr/local/include/libdecor-0 -I/usr/local/include -c unity.cpp -o ../build/engine.o
clang++ -Iinclude -Ithirdparty/imgui samples/hello/hello.cpp *.o ../build/engine.o -L/usr/local/lib -lvulkan -lwayland-client -lwayland-cursor -ldecor-0 -o ../build/hello
clang++ -Iinclude -Ithirdparty/imgui tools/editor/*.cpp *.o ../build/engine.o -L/usr/local/lib -lvulkan -lwayland-client -lwayland-cursor -ldecor-0 -o ../build/editor

