#include "window_win32.cpp"

#if _DEBUG
#define VK_CHECK( x ) { VkResult res = (x); teAssert( res == VK_SUCCESS ); }
#if _MSC_VER
#define teAssert( c ) if (!(c)) __debugbreak()
#else
#define teAssert( c ) if (!(c)) *(volatile int *)0 = 0
#endif
#else
#define teAssert( c )
#define VK_CHECK( x ) x
#endif


#include "core/camera.cpp"
#include "core/file.cpp"
#include "core/frustum.cpp"
#include "core/gameobject.cpp"
#include "core/math.cpp"
#include "core/scene.cpp"
#include "core/transform.cpp"
#include "mesh.cpp"
#include "vulkan/renderer_vulkan.cpp"
#include "vulkan/shader_vulkan.cpp"
#include "vulkan/texture_vulkan.cpp"
