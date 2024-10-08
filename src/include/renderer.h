#pragma once

// @param swapInterval Swap interval. 0 means VSync is off, 1 means that it's the display's frame rate.
// @param windowHandle Opaque pointer to the window handle. Pass the value that's returned by teCreateWindow().
// @param width Width of the swap chain in pixels.
// @param height Height of the swap chain in pixels.
void teCreateRenderer( unsigned swapInterval, void* windowHandle, unsigned width, unsigned height );
void teBeginFrame();
void teEndFrame();
// This must be called after loading all the meshes and before rendering them.
void teFinalizeMeshBuffers();
void teDrawFullscreenTriangle( struct teShader& shader, struct teTexture2D& texture, const struct ShaderParams& shaderParams, teBlendMode blendMode );
void teDrawQuad( const teShader& shader, teTexture2D texture, const ShaderParams& shaderParams, teBlendMode blendMode );
void teBeginSwapchainRendering();
void teEndSwapchainRendering();
void teMapUiMemory( void** outVertexMemory, void** outIndexMemory );
void teUnmapUiMemory();
void teUIDrawCall( const teShader& shader, const teTexture2D& fontTex, int displaySizeX, int displaySizeY, int scissorX, int scissorY, unsigned scissorW, unsigned scissorH, unsigned elementCount, unsigned indexOffset, unsigned vertexOffset );
// This is needed if you don't use an Xcode project that includes shaders.
void teLoadMetalShaderLibrary();

enum class teStat
{
    DrawCalls,
    PSOBinds,
};

float teRendererGetStat( teStat stat );
