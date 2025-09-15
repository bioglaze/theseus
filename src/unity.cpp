void UpdateUBO( const float localToClip[ 16 ], const float localToShadowClip[ 16 ], const float localToWorld[ 16 ], const struct ShaderParams& shaderParams, const struct Vec4& lightDirection, const Vec4& lightColor, const Vec4& lightPosition );

#if VK_USE_PLATFORM_WIN32_KHR
#include "window_win32.cpp"
#endif
#if VK_USE_PLATFORM_WAYLAND_KHR
#include "video/window_wayland.cpp"
#endif
#include "core/te_stdlib.cpp"
#include "core/camera.cpp"
#include "core/file.cpp"
#include "core/frustum.cpp"
#include "core/gameobject.cpp"
#include "core/math.cpp"
#include "core/scene.cpp"
#include "core/transform.cpp"
#include "video/light.cpp"
#include "material.cpp"
#include "mesh.cpp"
#include "textureloader.cpp"
#if VK_USE_PLATFORM_WIN32_KHR || VK_USE_PLATFORM_WAYLAND_KHR
#include "vulkan/buffer_vulkan.cpp"
#include "vulkan/renderer_vulkan.cpp"
#include "vulkan/shader_vulkan.cpp"
#include "vulkan/texture_vulkan.cpp"
#else
#include "metal/buffer_metal.cpp"
#include "metal/renderer_metal.cpp"
#include "metal/shader_metal.cpp"
#include "metal/texture_metal.cpp"
#endif
