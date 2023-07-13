#include "camera.h"
#include "file.h"
#include "gameobject.h"
#include "imgui.h"
#include "material.h"
#include "mesh.h"
#include "quaternion.h"
#include "renderer.h"
#include "scene.h"
#include "shader.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"
#include "window.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static LONGLONG PCFreq;

double GetMilliseconds()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter( &li );
    return (double)(li.QuadPart / (double)PCFreq);
}
#else
#include <time.h>

double GetMilliseconds()
{
    timespec spec;
    clock_gettime( CLOCK_MONOTONIC, &spec );
    return spec.tv_nsec / 1000000;
}
#endif

// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
struct pcg32_random_t
{
    uint64_t state;
    uint64_t inc;
};

uint32_t pcg32_random_r( pcg32_random_t* rng )
{
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
    uint32_t xorshifted = uint32_t( ((oldstate >> 18u) ^ oldstate) >> 27u );
    int32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

pcg32_random_t rng;

int Random100()
{
    return pcg32_random_r( &rng ) % 100;
}

struct ImGUIImplCustom
{
    int width = 0;
    int height = 0;
    const char* name = "jeejee";
};

void RenderImGUIDrawData( const teShader& shader, const teTexture2D& fontTex )
{
    ImDrawData* drawData = ImGui::GetDrawData();

    int fbWidth = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
    int fbHeight = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
    // Don't render when minimized.
    if (fbWidth <= 0 || fbHeight <= 0)
        return;

    if (drawData->TotalVtxCount > 0)
    {
        size_t vertex_size = drawData->TotalVtxCount * sizeof( ImDrawVert );
        size_t index_size = drawData->TotalIdxCount * sizeof( ImDrawIdx );

        void* vertexMemory = nullptr;
        void* indexMemory = nullptr;
        teMapUiMemory( &vertexMemory, &indexMemory );
        ImDrawVert* vtxDst = (ImDrawVert*)vertexMemory;
        ImDrawIdx* idxDst = (ImDrawIdx*)indexMemory;

        for (int n = 0; n < drawData->CmdListsCount; ++n)
        {
            const ImDrawList* cmd_list = drawData->CmdLists[ n ];
            memcpy( vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ) );
            memcpy( idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof( ImDrawIdx ) );
            vtxDst += cmd_list->VtxBuffer.Size;
            idxDst += cmd_list->IdxBuffer.Size;
        }
        
        teUnmapUiMemory();
    }

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = drawData->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = drawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    int global_vtx_offset = 0;
    int global_idx_offset = 0;

    for (int n = 0; n < drawData->CmdListsCount; ++n)
    {
        const ImDrawList* cmd_list = drawData->CmdLists[ n ];

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[ cmd_i ];

            if (pcmd->UserCallback != nullptr)
            {
                printf( "UserCallback not implemented\n" );
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                {
                    printf( "ImDrawCallback_ResetRenderState not implemented\n" );
                    //ImGui_ImplVulkan_SetupRenderState( draw_data, pipeline, command_buffer, rb, fb_width, fb_height );
                }
                else
                {
                    pcmd->UserCallback( cmd_list, pcmd );
                }
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min( (pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y );
                ImVec2 clip_max( (pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y );

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                if (clip_max.x > fbWidth) { clip_max.x = (float)fbWidth; }
                if (clip_max.y > fbHeight) { clip_max.y = (float)fbHeight; }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                teUIDrawCall( shader, fontTex, (int)drawData->DisplaySize.x, (int)drawData->DisplaySize.y, (int32_t)clip_min.x, (int32_t)clip_min.y, (uint32_t)(clip_max.x - clip_min.x), (uint32_t)(clip_max.y - clip_min.y), pcmd->ElemCount, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset );
            }
        }

        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }
}

int main()
{
#ifdef _WIN32
    LARGE_INTEGER li;
    QueryPerformanceFrequency( &li );
    PCFreq = li.QuadPart / 1000;
#endif
    unsigned width = 1920 / 1;
    unsigned height = 1080 / 1;
    void* windowHandle = teCreateWindow( width, height, "Theseus Engine Hello" );
    teCreateRenderer( 1, windowHandle, width, height );

    teFile unlitVsFile = teLoadFile( "shaders/unlit_vs.spv" );
    teFile unlitPsFile = teLoadFile( "shaders/unlit_ps.spv" );
    teShader unlitShader = teCreateShader( unlitVsFile, unlitPsFile, "unlitVS", "unlitPS" );

    teFile uiVsFile = teLoadFile( "shaders/ui_vs.spv" );
    teFile uiPsFile = teLoadFile( "shaders/ui_ps.spv" );
    teShader uiShader = teCreateShader( uiVsFile, uiPsFile, "uiVS", "uiPS" );

    teFile fullscreenVsFile = teLoadFile( "shaders/fullscreen_vs.spv" );
    teFile fullscreenPsFile = teLoadFile( "shaders/fullscreen_ps.spv" );
    teShader fullscreenShader = teCreateShader( fullscreenVsFile, fullscreenPsFile, "fullscreenVS", "fullscreenPS" );

    teFile skyboxVsFile = teLoadFile( "shaders/skybox_vs.spv" );
    teFile skyboxPsFile = teLoadFile( "shaders/skybox_ps.spv" );
    teShader skyboxShader = teCreateShader( skyboxVsFile, skyboxPsFile, "skyboxVS", "skyboxPS" );

    teGameObject camera3d = teCreateGameObject( "camera3d", teComponent::Transform | teComponent::Camera );
    Vec3 cameraPos = { 0, 0, -10 };
    Vec4 clearColor = { 1, 0, 0, 1 };
    teClearFlag clearFlag = teClearFlag::DepthAndColor;
    teTransformSetLocalPosition( camera3d.index, cameraPos );
    teCameraSetProjection( camera3d.index, 45, width / (float)height, 0.1f, 800.0f );
    teCameraSetClear( camera3d.index, clearFlag, clearColor );
    teCameraGetColorTexture( camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::BGRA_sRGB, "camera3d color" );
    teCameraGetDepthTexture( camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::Depth32F, "camera3d depth" );

    teFile gliderFile = teLoadFile( "assets/textures/glider_color.tga" );
    //teFile gliderFile = teLoadFile( "assets/textures/test/nonpow.tga" );
    teTexture2D gliderTex = teLoadTexture( gliderFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    teMaterial material = teCreateMaterial( unlitShader );
    teMaterialSetTexture2D( material, gliderTex, 0 );

    teFile transFile = teLoadFile( "assets/textures/font.tga" );
    teTexture2D transTex = teLoadTexture( transFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    teMaterial materialTrans = teCreateMaterial( unlitShader );
    materialTrans.blendMode = teBlendMode::Alpha;
    teMaterialSetTexture2D( materialTrans, transTex, 0 );

    teFile backFile = teLoadFile( "assets/textures/skybox/back.dds" );
    teFile frontFile = teLoadFile( "assets/textures/skybox/front.dds" );
    teFile leftFile = teLoadFile( "assets/textures/skybox/left.dds" );
    teFile rightFile = teLoadFile( "assets/textures/skybox/right.dds" );
    teFile topFile = teLoadFile( "assets/textures/skybox/top.dds" );
    teFile bottomFile = teLoadFile( "assets/textures/skybox/bottom.dds" );
    
    teTextureCube skyTex = teLoadTexture( leftFile, rightFile, bottomFile, topFile, frontFile, backFile, 0 );

    teFile bc1File = teLoadFile( "assets/textures/test/test_dxt1.dds" );
    teTexture2D bc1Tex = teLoadTexture( bc1File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    bc1Tex.sampler = teTextureSampler::NearestRepeat;
    teMaterialSetTexture2D( material, bc1Tex, 0 );

    teMesh cubeMesh = teCreateCubeMesh();
    teGameObject cubeGo = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
    teMeshRendererSetMesh( cubeGo.index, &cubeMesh );
    teMeshRendererSetMaterial( cubeGo.index, material, 0 );

    teMesh cubeMesh2 = teCreateCubeMesh();
    teGameObject cubeGo2 = teCreateGameObject( "cube2", teComponent::Transform | teComponent::MeshRenderer );
    teTransformSetLocalPosition( cubeGo2.index, Vec3( 0, 20, 0 ) );
    //teTransformSetLocalScale( cubeGo2.index, 2 );
    teMeshRendererSetMesh( cubeGo2.index, &cubeMesh2 );
    teMeshRendererSetMaterial( cubeGo2.index, materialTrans, 0 );

    teFinalizeMeshBuffers();

    teScene scene = teCreateScene();
    teSceneAdd( scene, camera3d.index );
    teSceneAdd( scene, cubeGo.index );
    teSceneAdd( scene, cubeGo2.index );

    teGameObject cubes[ 16 * 4 ];
    unsigned g = 0;
    Quaternion rotation;

    for (int j = 0; j < 4; ++j)
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int k = 0; k < 4; ++k)
            {
                cubes[ g ] = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
                teMeshRendererSetMesh( cubes[ g ].index, &cubeMesh );
                teMeshRendererSetMaterial( cubes[ g ].index, material, 0 );
                teTransformSetLocalPosition( cubes[ g ].index, Vec3( i * 4.0f - 5.0f, j * 4.0f - 5.0f, -4.0f - k * 4.0f ) );
                teSceneAdd( scene, cubes[ g ].index );

                float angle = Random100() / 100.0f * 90;
                Vec3 axis{ 1, 1, 1 };
                axis.Normalize();

                rotation.FromAxisAngle( axis, angle );
                teTransformSetLocalRotation( cubes[ g ].index, rotation );
                
                ++g;
            }
        }
    }

    ImGUIImplCustom impl;

    ImGuiContext* imContext = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = (float)width;
    io.DisplaySize.y = (float)height;
    ImGui::StyleColorsDark();
    
    unsigned char* fontPixels;
    int fontWidth, fontHeight;
    io.Fonts->GetTexDataAsRGBA32( &fontPixels, &fontWidth, &fontHeight );
    teFile nullFile;
    teTexture2D fontTex = teLoadTexture( nullFile, 0, fontPixels, fontWidth, fontHeight, teTextureFormat::RGBA_sRGB );

    io.BackendRendererUserData = &impl;
    io.BackendRendererName = "imgui_impl_vulkan";
    //io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    bool shouldQuit = false;
    bool isRightMouseDown = false;
    int x = 0;
    int y = 0;
    float deltaX = 0;
    float deltaY = 0;
    int lastMouseX = 0;
    int lastMouseY = 0;
    Vec3 moveDir;

    double theTime = GetMilliseconds();

    while (!shouldQuit)
    {
        double lastTime = theTime;
        theTime = GetMilliseconds();
        double dt = theTime - lastTime;

        tePushWindowEvents();

        bool eventsHandled = false;

        while (!eventsHandled)
        {
            const teWindowEvent& event = tePopWindowEvent();
            
            if (event.type == teWindowEvent::Type::Empty)
            {
                eventsHandled = true;
            }

            if ((event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Escape) || event.type == teWindowEvent::Type::Close)
            {
                shouldQuit = true;
            }

            if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::R)
            {
                teHotReload();
            }
            else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::S)
            {
                moveDir.z = -0.5f;
            }
            else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::S)
            {
                moveDir.z = 0;
            }
            else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::W)
            {
                moveDir.z = 0.5f;
            }
            else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::W)
            {
                moveDir.z = 0;
            }
            else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::A)
            {
                moveDir.x = 0.5f;
                //io.AddKeyEvent( ImGuiKey_A, true );
            }
            else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::A)
            {
                moveDir.x = 0;
            }
            else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::D)
            {
                moveDir.x = -0.5f;
            }
            else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::D)
            {
                moveDir.x = 0;
            }
            else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Q)
            {
                moveDir.y = 0.5f;
            }
            else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::Q)
            {
                moveDir.y = 0;
            }
            else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::E)
            {
                moveDir.y = -0.5f;
            }
            else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::E)
            {
                moveDir.y = 0;
            }
            else if (event.type == teWindowEvent::Type::Mouse1Down)
            {
                x = event.x;
                y = height - event.y;
                //isRightMouseDown = true;
                lastMouseX = x;
                lastMouseY = y;
                deltaX = 0;
                deltaY = 0;

                io.AddMouseButtonEvent( 0, true );
            }
            else if (event.type == teWindowEvent::Type::Mouse1Up)
            {
                x = event.x;
                y = height - event.y;
                //isRightMouseDown = false;
                deltaX = 0;
                deltaY = 0;
                lastMouseX = x;
                lastMouseY = y;

                io.AddMouseButtonEvent( 0, false );
            }
            else if (event.type == teWindowEvent::Type::Mouse2Down)
            {
                x = event.x;
                y = height - event.y;
                isRightMouseDown = true;
                lastMouseX = x;
                lastMouseY = y;
                deltaX = 0;
                deltaY = 0;
            }
            else if (event.type == teWindowEvent::Type::Mouse2Up)
            {
                x = event.x;
                y = height - event.y;
                isRightMouseDown = false;
                deltaX = 0;
                deltaY = 0;
                lastMouseX = x;
                lastMouseY = y;
            }
            else if (event.type == teWindowEvent::Type::MouseMove)
            {
                x = event.x;
                y = height - event.y;
                deltaX = float( x - lastMouseX );
                deltaY = float( y - lastMouseY );
                lastMouseX = x;
                lastMouseY = y;

                if (isRightMouseDown)
                {
                    teTransformOffsetRotate( camera3d.index, Vec3( 0, 1, 0 ), -deltaX / 100.0f * (float)dt );
                    teTransformOffsetRotate( camera3d.index, Vec3( 1, 0, 0 ), -deltaY / 100.0f * (float)dt );
                }

                io.AddMousePosEvent( x, y );
            }
        }

        Vec3 oldCameraPos = teTransformGetLocalPosition( camera3d.index );

        teTransformMoveForward( camera3d.index, moveDir.z * (float)dt * 0.5f );
        teTransformMoveRight( camera3d.index, moveDir.x * (float)dt * 0.5f );
        teTransformMoveUp( camera3d.index, moveDir.y * (float)dt * 0.5f );

        teBeginFrame();
        ImGui::NewFrame();
        teSceneRender( scene, &skyboxShader, &skyTex, &cubeMesh );

        cameraPos = -teTransformGetLocalPosition( camera3d.index );

        if (teScenePointInsideAABB( scene, cameraPos ))
        {
            teTransformSetLocalPosition( camera3d.index, oldCameraPos );
        }

        ImGui::Begin( "ImGUI" );
        ImGui::Text( "This is some useful text." );
        ImGui::End();
        ImGui::Render();

        teBeginSwapchainRendering( teCameraGetColorTexture( camera3d.index ) );
        RenderImGUIDrawData( uiShader, fontTex );
        teDrawFullscreenTriangle( fullscreenShader, teCameraGetColorTexture( camera3d.index ) );
        teEndSwapchainRendering();

        teEndFrame();
    }

    free( unlitVsFile.data );
    free( unlitPsFile.data );
    free( uiVsFile.data );
    free( uiPsFile.data );
    free( fullscreenVsFile.data );
    free( fullscreenPsFile.data );
    free( skyboxVsFile.data );
    free( skyboxPsFile.data );
    free( gliderFile.data );
    free( backFile.data );
    free( frontFile.data );
    free( leftFile.data );
    free( rightFile.data );
    free( topFile.data );
    free( bottomFile.data );
    free( bc1File.data );

    ImGui::DestroyContext( imContext );

    return 0;
}
