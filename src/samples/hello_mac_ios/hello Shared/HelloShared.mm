#include "camera.h"
#include "file.h"
#include "gameobject.h"
#include "imgui.h"
#include "material.h"
#include "mesh.h"
#include "renderer.h"
#include "quaternion.h"
#include "scene.h"
#include "shader.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"
#include <stdint.h>

struct ImGUIImplCustom
{
    int width = 0;
    int height = 0;
    const char* name = "jeejee";
};

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

void RenderImGUIDrawData( const teShader& shader, const teTexture2D& fontTex )
{
    ImDrawData* drawData = ImGui::GetDrawData();
    drawData->FramebufferScale.x = 2;
    drawData->FramebufferScale.y = 2;
    int fbWidth = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
    int fbHeight = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
    // Don't render when minimized.
    if (fbWidth <= 0 || fbHeight <= 0)
        return;

    if (drawData->TotalVtxCount > 0)
    {
        //size_t vertex_size = drawData->TotalVtxCount * sizeof( ImDrawVert );
        //size_t index_size = drawData->TotalIdxCount * sizeof( ImDrawIdx );

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
                //printf( "UserCallback not implemented\n" );
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                {
                    //printf( "ImDrawCallback_ResetRenderState not implemented\n" );
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

struct AppResources
{
    teShader unlitShader;
    teShader fullscreenShader;
    teShader fullscreenAdditiveShader;
    teShader skyboxShader;
    teShader uiShader;
    teShader standardShader;
    teShader bloomThresholdShader;
    teShader bloomBlurShader;
    teShader momentsShader;
    
    teGameObject camera3d;
    teGameObject cubeGo;
    teGameObject cubes[ 16 * 4 ];
    teMaterial material;
    teMaterial standardMaterial;
    teMesh cubeMesh;
    teTexture2D gliderTex;
    teTexture2D bc1Tex;
    teTexture2D bc2Tex;
    teTexture2D bc3Tex;
    teTextureCube skyTex;
    teTexture2D fontTex;
    teTexture2D bloomTarget;
    teScene scene;
    
    ImGUIImplCustom impl;
};

AppResources app;

Vec3 moveDir;

void MoveForward( float amount )
{
    moveDir.z = 0.3f * amount;
}

void MoveRight( float amount )
{
    moveDir.x = 0.3f * amount;
}

void MoveUp( float amount )
{
    moveDir.y = 0.3f * amount;
}

void RotateCamera( float x, float y )
{
    teTransformOffsetRotate( app.camera3d.index, Vec3( 0, 1, 0 ), -x / 20 );
    teTransformOffsetRotate( app.camera3d.index, Vec3( 1, 0, 0 ), -y / 20 );
}

void InitApp( unsigned width, unsigned height )
{
    teCreateRenderer( 1, nullptr, width, height );

    app.fullscreenShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "fullscreenVS", "fullscreenPS" );
    app.fullscreenAdditiveShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "fullscreenVS", "fullscreenAdditivePS" );
    app.unlitShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "unlitVS", "unlitPS" );
    app.skyboxShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "skyboxVS", "skyboxPS" );
    app.uiShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "uiVS", "uiPS" );
    app.standardShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "standardVS", "standardPS" );
    app.momentsShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "momentsVS", "momentsPS" );
    
    app.camera3d = teCreateGameObject( "camera3d", teComponent::Transform | teComponent::Camera );
    Vec3 cameraPos = { 0, 0, -10 };
    teTransformSetLocalPosition( app.camera3d.index, cameraPos );
    teCameraSetProjection( app.camera3d.index, 45, width / (float)height, 0.1f, 800.0f );
    teCameraSetClear( app.camera3d.index, teClearFlag::DepthAndColor, Vec4( 1, 0, 0, 1 ) );
    teCameraGetColorTexture( app.camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::BGRA_sRGB, "camera3d color" );
    teCameraGetDepthTexture( app.camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::Depth32F, "camera3d depth" );
    app.bloomTarget = teCreateTexture2D( width, height, teTextureFlags::UAV, teTextureFormat::R32F, "bloom target" );
    
    app.gliderTex = teLoadTexture( teLoadFile( "assets/textures/glider_color.tga" ), teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    app.bc1Tex = teLoadTexture( teLoadFile( "assets/textures/test/test_dxt1.dds" ), teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    app.bc2Tex = teLoadTexture( teLoadFile( "assets/textures/test/test_dxt3.dds" ), teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    app.bc3Tex = teLoadTexture( teLoadFile( "assets/textures/test/test_dxt5.dds" ), teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

    app.material = teCreateMaterial( app.unlitShader );
    teMaterialSetTexture2D( app.material, app.bc1Tex, 0 );

    app.standardMaterial = teCreateMaterial( app.standardShader );
    teMaterialSetTexture2D( app.standardMaterial, app.gliderTex, 0 );

    app.cubeMesh = teCreateCubeMesh();
    app.cubeGo = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
    teMeshRendererSetMesh( app.cubeGo.index, &app.cubeMesh );
    teMeshRendererSetMaterial( app.cubeGo.index, app.standardMaterial, 0 );
    teTransformSetLocalScale( app.cubeGo.index, 4 );
    teTransformSetLocalPosition( app.cubeGo.index, Vec3( 0, -4, 0 ) );
    
    teFile backFile = teLoadFile( "assets/textures/skybox/back.dds" );
    teFile frontFile = teLoadFile( "assets/textures/skybox/front.dds" );
    teFile leftFile = teLoadFile( "assets/textures/skybox/left.dds" );
    teFile rightFile = teLoadFile( "assets/textures/skybox/right.dds" );
    teFile topFile = teLoadFile( "assets/textures/skybox/top.dds" );
    teFile bottomFile = teLoadFile( "assets/textures/skybox/bottom.dds" );
    
    teFile emptyFile;
    app.bloomThresholdShader = teCreateComputeShader( emptyFile, "bloomThreshold", 16, 16 );
    app.bloomBlurShader = teCreateComputeShader( emptyFile, "bloomBlur", 16, 16 );

    app.skyTex = teLoadTexture( leftFile, rightFile, bottomFile, topFile, frontFile, backFile, 0 );

    app.scene = teCreateScene( 2048 );
    teSceneAdd( app.scene, app.camera3d.index );
    teSceneAdd( app.scene, app.cubeGo.index );
    
    unsigned g = 0;
    Quaternion rotation;

    for (int j = 0; j < 4; ++j)
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int k = 0; k < 4; ++k)
            {
                app.cubes[ g ] = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
                teMeshRendererSetMesh( app.cubes[ g ].index, &app.cubeMesh );
                teMeshRendererSetMaterial( app.cubes[ g ].index, app.material, 0 );
                teTransformSetLocalPosition( app.cubes[ g ].index, Vec3( i * 4.0f - 5.0f, j * 4.0f - 5.0f, -4.0f - k * 4.0f ) );
                teSceneAdd( app.scene, app.cubes[ g ].index );

                float angle = Random100() / 100.0f * 90;
                Vec3 axis{ 1, 1, 1 };
                axis.Normalize();

                rotation.FromAxisAngle( axis, angle );
                teTransformSetLocalRotation( app.cubes[ g ].index, rotation );
                
                ++g;
            }
        }
    }

    teFinalizeMeshBuffers();

    /*ImGuiContext* imContext =*/ ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = (float)width;
    io.DisplaySize.y = (float)height;
    io.IniFilename = NULL;
    ImGui::StyleColorsDark();
    unsigned char* fontPixels;
    int fontWidth, fontHeight;
    io.Fonts->GetTexDataAsRGBA32( &fontPixels, &fontWidth, &fontHeight );
    teFile nullFile;
    strcpy( nullFile.path, "fontTexture" );
    app.fontTex = teLoadTexture( nullFile, 0, fontPixels, fontWidth, fontHeight, teTextureFormat::RGBA_sRGB );
    io.BackendRendererUserData = &app.impl;
    io.BackendRendererName = "imgui_impl_metal";
}

void MouseDown( int x, int y )
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent( 0, true );
}

void MouseUp( int x, int y )
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent( 0, false );
}

void MouseMove( int x, int y )
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent( x, y );
}

void DrawApp()
{
    float dt = 1;

    ShaderParams shaderParams;

    teTransformMoveForward( app.camera3d.index, moveDir.z * (float)dt * 0.5f );
    teTransformMoveRight( app.camera3d.index, moveDir.x * (float)dt * 0.5f );
    teTransformMoveUp( app.camera3d.index, moveDir.y * (float)dt * 0.5f );

    teBeginFrame();
    ImGui::NewFrame();
    teSceneRender( app.scene, &app.skyboxShader, &app.skyTex, &app.cubeMesh, app.momentsShader, Vec3( 0, 0, -10 ) );

    unsigned width, height;
    teTextureGetDimension( app.bloomTarget, width, height );
    shaderParams.readTexture = teCameraGetColorTexture( app.camera3d.index ).index;
    shaderParams.writeTexture = app.bloomTarget.index;
    shaderParams.bloomThreshold = 0.8f;
    teShaderDispatch( app.bloomThresholdShader, width / 16, height / 16, 1, shaderParams, "bloom threshold" );
    
    ImGui::Begin( "ImGUI" );
    ImGui::Text( "draw calls: %.0f\nPSO binds: %.0f", teRendererGetStat( teStat::DrawCalls ), teRendererGetStat( teStat::PSOBinds ) );
    ImGui::End();
    ImGui::Render();

    teBeginSwapchainRendering();
    
    shaderParams.tilesXY[ 0 ] = 2.0f;
    shaderParams.tilesXY[ 1 ] = 2.0f;
    shaderParams.tilesXY[ 2 ] = -1.0f;
    shaderParams.tilesXY[ 3 ] = -1.0f;
    teDrawQuad( app.fullscreenShader, teCameraGetColorTexture( app.camera3d.index ), shaderParams, teBlendMode::Off );
    
    shaderParams.tilesXY[ 0 ] = 4.0f;
    shaderParams.tilesXY[ 1 ] = 4.0f;
    shaderParams.tilesXY[ 2 ] = -1.0f;
    shaderParams.tilesXY[ 3 ] = -1.0f;
    teDrawQuad( app.fullscreenAdditiveShader, app.bloomTarget, shaderParams, teBlendMode::Additive );

    RenderImGUIDrawData( app.uiShader, app.fontTex );
    teEndSwapchainRendering();
    
    //RotateCamera( -2, -2 );
}
