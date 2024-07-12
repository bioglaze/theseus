#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "camera.h"
#include "file.h"
#include "gameobject.h"
#include "light.h"
#include "material.h"
#include "matrix.h"
#include "mesh.h"
#include "quaternion.h"
#include "renderer.h"
#include "scene.h"
#include "shader.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"
#include "window.h"
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "imgui.h"

void GetOpenPath( char* path, const char* extension );

constexpr int MaxGameObjects = 100;

struct SceneView
{
    unsigned width;
    unsigned height;

    teShader unlitShader;
    teShader fullscreenShader;
    teShader fullscreenAdditiveShader;
    teShader momentsShader;
    teGameObject cubeGo;
    teGameObject translateGizmoGo;

    teTexture2D fontTex;
    teTexture2D gliderTex;

    teMesh cubeMesh;
    teMesh translateGizmoMesh;
    teMaterial material;
    teScene scene;

    teShader skyboxShader;

    teShader uiShader;

    teTextureCube skyTex;
    teGameObject camera3d;
};

SceneView sceneView;

struct ImGUIImplCustom
{
    int width = 0;
    int height = 0;
    const char* name = "jeejee";
};

ImGUIImplCustom impl;

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

void ScreenPointToRay( int screenX, int screenY, float screenWidth, float screenHeight, teGameObject camera, Vec3& outRayOrigin, Vec3& outRayTarget )
{
    const float aspect = screenHeight / screenWidth;
    const float halfWidth = screenWidth * 0.5f;
    const float halfHeight = screenHeight * 0.5f;
    const float fov = teCameraGetFovDegrees( camera.index ) * (3.1415926535f / 180.0f);

    // Normalizes screen coordinates and scales them to the FOV.
    const float dx = tanf( fov * 0.5f ) * (screenX / halfWidth - 1.0f) / aspect;
    const float dy = tanf( fov * 0.5f ) * (screenY / halfHeight - 1.0f);
    
    Matrix view = teTransformGetMatrix( camera.index );
    Vec3 pos = teTransformGetLocalPosition( camera.index );
    
    Matrix translation;
    translation.SetTranslation( -pos );
    Matrix::Multiply( translation, view, view );

    Matrix invView;
    // FIXME: does this need to be a proper invert or will a simpler function do?
    Matrix::Invert( view, invView );

    const float farp = teCameraGetFar( camera.index );

    outRayOrigin = pos;
    outRayTarget = -Vec3( -dx * farp, dy * farp, farp );

    Matrix::TransformPoint( outRayTarget, invView, outRayTarget );
}

void InitSceneView( unsigned width, unsigned height, void* windowHandle, int uiScale )
{
    teCreateRenderer( 1, windowHandle, width, height );

    sceneView.width = width;
    sceneView.height = height;

    teFile unlitVsFile = teLoadFile( "shaders/unlit_vs.spv" );
    teFile unlitPsFile = teLoadFile( "shaders/unlit_ps.spv" );
    sceneView.unlitShader = teCreateShader( unlitVsFile, unlitPsFile, "unlitVS", "unlitPS" );

    teFile fullscreenVsFile = teLoadFile( "shaders/fullscreen_vs.spv" );
    teFile fullscreenPsFile = teLoadFile( "shaders/fullscreen_ps.spv" );
    teFile fullscreenAdditivePsFile = teLoadFile( "shaders/fullscreen_additive_ps.spv" );
    sceneView.fullscreenShader = teCreateShader( fullscreenVsFile, fullscreenPsFile, "fullscreenVS", "fullscreenPS" );
    sceneView.fullscreenAdditiveShader = teCreateShader( fullscreenVsFile, fullscreenAdditivePsFile, "fullscreenVS", "fullscreenAdditivePS" );

    teFile skyboxVsFile = teLoadFile( "shaders/skybox_vs.spv" );
    teFile skyboxPsFile = teLoadFile( "shaders/skybox_ps.spv" );
    sceneView.skyboxShader = teCreateShader( skyboxVsFile, skyboxPsFile, "skyboxVS", "skyboxPS" );

    teFile uiVsFile = teLoadFile( "shaders/ui_vs.spv" );
    teFile uiPsFile = teLoadFile( "shaders/ui_ps.spv" );
    sceneView.uiShader = teCreateShader( uiVsFile, uiPsFile, "uiVS", "uiPS" );

    teFile momentsVsFile = teLoadFile( "shaders/moments_vs.spv" );
    teFile momentsPsFile = teLoadFile( "shaders/moments_ps.spv" );
    sceneView.momentsShader = teCreateShader( momentsVsFile, momentsPsFile, "momentsVS", "momentsPS" );
    
    teFile backFile = teLoadFile( "assets/textures/skybox/back.dds" );
    teFile frontFile = teLoadFile( "assets/textures/skybox/front.dds" );
    teFile leftFile = teLoadFile( "assets/textures/skybox/left.dds" );
    teFile rightFile = teLoadFile( "assets/textures/skybox/right.dds" );
    teFile topFile = teLoadFile( "assets/textures/skybox/top.dds" );
    teFile bottomFile = teLoadFile( "assets/textures/skybox/bottom.dds" );

    sceneView.skyTex = teLoadTexture( leftFile, rightFile, bottomFile, topFile, frontFile, backFile, 0 );

    sceneView.camera3d = teCreateGameObject( "camera3d", teComponent::Transform | teComponent::Camera );
    Vec3 cameraPos = { 0, 0, -10 };
    Vec4 clearColor = { 1, 0, 0, 1 };
    teClearFlag clearFlag = teClearFlag::DepthAndColor;
    teTransformSetLocalPosition( sceneView.camera3d.index, cameraPos );
    teCameraSetProjection( sceneView.camera3d.index, 45, width / (float)height, 0.1f, 800.0f );
    teCameraSetClear( sceneView.camera3d.index, clearFlag, clearColor );
    teCameraGetColorTexture( sceneView.camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::BGRA_sRGB, "camera3d color" );
    teCameraGetDepthTexture( sceneView.camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::Depth32F, "camera3d depth" );

    teFile gliderFile = teLoadFile( "assets/textures/glider_color.tga" );
    sceneView.gliderTex = teLoadTexture( gliderFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    sceneView.material = teCreateMaterial( sceneView.unlitShader );
    teMaterialSetTexture2D( sceneView.material, sceneView.gliderTex, 0 );

    sceneView.cubeMesh = teCreateCubeMesh();
    sceneView.cubeGo = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
    teTransformSetLocalPosition( sceneView.cubeGo.index, Vec3( 0, 0, 0 ) );
    //teTransformSetLocalScale( cubeGo2.index, 2 );
    teMeshRendererSetMesh( sceneView.cubeGo.index, &sceneView.cubeMesh );
    teMeshRendererSetMaterial( sceneView.cubeGo.index, sceneView.material, 0 );

    teFile gizmoFile = teLoadFile( "assets/meshes/translation_gizmo.t3d" );
    sceneView.translateGizmoMesh = teLoadMesh( gizmoFile );
    sceneView.translateGizmoGo = teCreateGameObject( "translationGizmo", teComponent::Transform | teComponent::MeshRenderer );
    teTransformSetLocalPosition( sceneView.translateGizmoGo.index, Vec3( 2, 0, 0 ) );
    teTransformSetLocalScale( sceneView.translateGizmoGo.index, 0.25f );
    teMeshRendererSetMesh( sceneView.translateGizmoGo.index, &sceneView.translateGizmoMesh );
    teMeshRendererSetMaterial( sceneView.translateGizmoGo.index, sceneView.material, 0 );

    sceneView.scene = teCreateScene( 2048 );
    teFinalizeMeshBuffers();

    teSceneAdd( sceneView.scene, sceneView.camera3d.index );
    teSceneAdd( sceneView.scene, sceneView.translateGizmoGo.index );
    teSceneAdd( sceneView.scene, sceneView.cubeGo.index );
    teSceneSetupDirectionalLight( sceneView.scene, Vec3( 1, 1, 1 ), Vec3( 0, 1, 0 ) );

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = (float)width * uiScale;
    io.DisplaySize.y = (float)height * uiScale;
    io.FontGlobalScale = 2;
    ImGui::StyleColorsDark();

    unsigned char* fontPixels;
    int fontWidth, fontHeight;
    io.Fonts->GetTexDataAsRGBA32( &fontPixels, &fontWidth, &fontHeight );
    teFile nullFile;
    sceneView.fontTex = teLoadTexture( nullFile, 0, fontPixels, fontWidth, fontHeight, teTextureFormat::RGBA_sRGB );

    io.BackendRendererUserData = &impl;
    io.BackendRendererName = "imgui_impl_vulkan";
}

unsigned SceneViewGetCameraIndex()
{
    return sceneView.camera3d.index;
}

char text[ 100 ];
char openFilePath[ 280 ];
unsigned selectedGoIndex = 1; // 1 is editor camera which the UI considers as non-selection.
float pos[ 3 ];
float scale = 1;

void RenderSceneView()
{
    teBeginFrame();
    ImGui::NewFrame();
    Vec3 dirLightShadowCasterPosition;
    teSceneRender( sceneView.scene, &sceneView.skyboxShader, &sceneView.skyTex, &sceneView.cubeMesh, sceneView.momentsShader, dirLightShadowCasterPosition );

    teBeginSwapchainRendering();

    ShaderParams shaderParams{};
    shaderParams.tilesXY[ 0 ] = 2.0f;
    shaderParams.tilesXY[ 1 ] = 2.0f;
    shaderParams.tilesXY[ 2 ] = -1.0f;
    shaderParams.tilesXY[ 3 ] = -1.0f;
    teDrawQuad( sceneView.fullscreenShader, teCameraGetColorTexture( sceneView.camera3d.index ), shaderParams, teBlendMode::Off );
    shaderParams.tilesXY[ 0 ] = 4.0f;
    shaderParams.tilesXY[ 1 ] = 4.0f;
    //teDrawQuad( fullscreenAdditiveShader, bloomTarget, shaderParams, teBlendMode::Additive );

    if (ImGui::Begin( "Hierarchy" ))
    {
        if (ImGui::Button( "Add Game object" ))
        {
            teGameObject go = teCreateGameObject( "gameobject", teComponent::Transform );

            teSceneAdd( sceneView.scene, go.index );
        }

        ImGui::Text( "Game objects:" );

        unsigned goCount = teSceneGetMaxGameObjects();

        for (unsigned i = 0; i < goCount; ++i)
        {
            unsigned goIndex = teSceneGetGameObjectIndex( sceneView.scene, i );
            
            if (goIndex != 0 && goIndex != sceneView.translateGizmoGo.index && goIndex != sceneView.camera3d.index)
            {
                ImGui::Text( "%s", teGameObjectGetName( goIndex ) );
                
                if (ImGui::IsItemClicked())
                {
                    printf( "clicked %s\n", teGameObjectGetName( goIndex ) );
                    selectedGoIndex = goIndex;
                }
            }
        }
    }

    ImGui::End();

    if (ImGui::Begin( "Inspector" ))
    {
        if (selectedGoIndex != sceneView.translateGizmoGo.index && selectedGoIndex != sceneView.camera3d.index)
        {
            ImGui::Text( "%s", teGameObjectGetName( selectedGoIndex ) );
         
            if (ImGui::CollapsingHeader( "Transform" ))
            //ImGui::SeparatorText( "Transform" );
            {
                ImGui::InputFloat3( "position", (float*)teTransformAccessLocalPosition( selectedGoIndex ), "%.3f", ImGuiInputTextFlags_CharsScientific );
                ImGui::InputFloat( "scale", teTransformAccessLocalScale( selectedGoIndex ), 0, 0, "%.3f", ImGuiInputTextFlags_CharsScientific );
            }

            if (teGameObjectGetComponents( selectedGoIndex ) & teComponent::MeshRenderer)
            {
                if (ImGui::CollapsingHeader( "Mesh Renderer" ))
                {
                    ImGui::Text( "path" ); // TODO: tooltip shows full path, without tooltip shows only file name.
                    ImGui::SameLine();
                    
                    if (ImGui::Button( "Load" ))
                    {
                        openFilePath[ 0 ] = 0;
                        GetOpenPath( openFilePath, "t3d" );

                        if (openFilePath[ 0 ] != 0)
                        {
                            //teMeshRendererSetMesh( selectedGoIndex, mesh );
                        }
                    }
                }
            }
            else if (ImGui::Button( "Add Mesh Renderer" ))
            {
                printf( "added mesh renderer\n" );
            }
        }
    }

    ImGui::End();

    ImGui::Render();

    RenderImGUIDrawData( sceneView.uiShader, sceneView.fontTex );
    teEndSwapchainRendering();

    teEndFrame();
}

void RotateEditorCamera( float x, float y )
{
    teTransformOffsetRotate( sceneView.camera3d.index, Vec3( 0, 1, 0 ), -x / 20 );
    teTransformOffsetRotate( sceneView.camera3d.index, Vec3( 1, 0, 0 ), -y / 20 );
}

void MoveEditorCamera( float right, float up, float forward )
{
    teTransformMoveForward( sceneView.camera3d.index, forward );
    teTransformMoveUp( sceneView.camera3d.index, up );
    teTransformMoveRight( sceneView.camera3d.index, right );
}
