#if VK_USE_PLATFORM_WIN32_KHR
#include "window_win32.cpp"
#endif
#if VK_USE_PLATFORM_WAYLAND_KHR
#include "video/window_wayland.cpp"
#endif
#include "core/camera.cpp"
#include "core/file.cpp"
#include "core/frustum.cpp"
#include "core/gameobject.cpp"
#include "core/math.cpp"
#include "core/scene.cpp"
#include "core/transform.cpp"
#include "material.cpp"
#include "mesh.cpp"
#include "textureloader.cpp"
#include "vulkan/buffer_vulkan.cpp"
#include "vulkan/renderer_vulkan.cpp"
#include "vulkan/shader_vulkan.cpp"
#include "vulkan/texture_vulkan.cpp"
