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
#include "mathutil.h"
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

constexpr unsigned MaxSelectedObjects = 10;

constexpr unsigned EditorCameraGoIndex = 1;

char text[ 100 ];
char openFilePath[ 280 ];
unsigned selectedGoIndex = EditorCameraGoIndex;
int gizmoAxisSelected = -1;
int gizmoAxisHovered = -1;
float pos[ 3 ];
float scale = 1;
float lightDir[ 3 ] = { 0.02f, -1, 0.02f };
float lightColor[ 3 ] = { 1, 1, 1 };

struct SceneView
{
    unsigned width;
    unsigned height;

    teShader unlitShader;
    teShader standardShader;
    teShader fullscreenShader;
    teShader fullscreenAdditiveShader;
    teShader momentsShader;
    teGameObject cubeGo;
    teGameObject translateGizmoGo;

    teTexture2D fontTex;
    teTexture2D gliderTex;
    teTexture2D redTex;
    teTexture2D greenTex;
    teTexture2D blueTex;

    teMesh cubeMesh;
    teMesh translateGizmoMesh;
    teMaterial material;
    teMaterial standardMaterial;
    teMaterial redMaterial;
    teMaterial greenMaterial;
    teMaterial blueMaterial;
    teScene scene;

    teShader skyboxShader;

    teShader uiShader;

    teTextureCube skyTex;
    teGameObject camera3d;
    teGameObject selectedGos[ MaxSelectedObjects ];
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

void GetColliders( unsigned screenX, unsigned screenY, bool skipGizmo, int& outClosestSceneGo, unsigned& outClosestSubMesh )
{
    Vec3 rayOrigin, rayTarget;
    ScreenPointToRay( screenX, screenY, (float)sceneView.width, (float)sceneView.height, sceneView.camera3d.index, rayOrigin, rayTarget );

    outClosestSceneGo = -1;
    float closestDistance = 99999.0f;
    outClosestSubMesh = 666;

    for (unsigned go = 0; go < teSceneGetMaxGameObjects(); ++go)
    {
        unsigned sceneGo = teSceneGetGameObjectIndex( sceneView.scene, go );

        if ((teGameObjectGetComponents( sceneGo ) & teComponent::MeshRenderer) == 0)
        {
            continue;
        }

        if (skipGizmo && sceneGo == sceneView.translateGizmoGo.index)
        {
            continue;
        }

        for (unsigned subMesh = 0; subMesh < teMeshGetSubMeshCount( teMeshRendererGetMesh( sceneGo ) ); ++subMesh)
        {
            Vec3 mMinLocal, mMaxLocal;
            Vec3 mMinWorld, mMaxWorld;
            Vec3 mAABB[ 8 ];

            teMeshGetSubMeshLocalAABB( *teMeshRendererGetMesh( sceneGo ), subMesh, mMinLocal, mMaxLocal );
            teGetCorners( mMinLocal, mMaxLocal, mAABB );

            for (int v = 0; v < 8; ++v)
            {
                Matrix::TransformPoint( mAABB[ v ], teTransformGetMatrix( sceneGo ), mAABB[ v ] );
            }

            GetMinMax( mAABB, 8, mMinWorld, mMaxWorld );

            const float meshDistance = IntersectRayAABB( rayOrigin, rayTarget, mMinWorld, mMaxWorld );
            
            if (meshDistance > 0 && meshDistance < closestDistance)
            {
                closestDistance = meshDistance;
                outClosestSceneGo = sceneGo;
                outClosestSubMesh = subMesh;
            }
        }
    }
}

void SelectObject( unsigned x, unsigned y )
{
    selectedGoIndex = EditorCameraGoIndex;
    unsigned closestSubMesh = 666;
    int closestSceneGo = -1;

    GetColliders( x, y, true, closestSceneGo, closestSubMesh );

    if (closestSceneGo != -1)
    {
        selectedGoIndex = closestSceneGo;
        teTransformSetLocalPosition( sceneView.translateGizmoGo.index, teTransformGetLocalPosition( selectedGoIndex ) );
    }

    teMeshRendererSetEnabled( sceneView.translateGizmoGo.index, closestSceneGo != -1 );

    sceneView.selectedGos[ 0 ].index = selectedGoIndex;
}

void SceneMoveSelection( Vec3 amount )
{
    teTransformMoveRight( selectedGoIndex, amount.x );
    teTransformMoveUp( selectedGoIndex, amount.y );
    teTransformSetLocalPosition( sceneView.translateGizmoGo.index, teTransformGetLocalPosition( selectedGoIndex ) );
}

void SceneViewDuplicate()
{
    if (selectedGoIndex != EditorCameraGoIndex)
    {
        const unsigned components = teGameObjectGetComponents( selectedGoIndex );
        teGameObject go = teCreateGameObject( "gameobject", components );
        teTransformSetLocalPosition( go.index, teTransformGetLocalPosition( selectedGoIndex ) );

        if (components & teComponent::MeshRenderer)
        {
            teMeshRendererSetMesh( go.index, teMeshRendererGetMesh( selectedGoIndex ) );
            teMeshRendererSetEnabled( go.index, teMeshRendererIsEnabled( selectedGoIndex ) );

            for (unsigned i = 0; i < teMeshGetSubMeshCount( teMeshRendererGetMesh( selectedGoIndex ) ); ++i)
            {
                teMeshRendererSetMaterial( go.index, teMeshRendererGetMaterial( selectedGoIndex, i ), i );
            }
        }

        teSceneAdd( sceneView.scene, go.index );
    }
}

void SceneMouseMove( float x, float y, float dx, float dy, bool isLeftMouseDown )
{
    teMaterialSetTint( sceneView.greenMaterial, { 1, 1, 1, 1 } );
    teMaterialSetTint( sceneView.redMaterial, { 1, 1, 1, 1 } );
    teMaterialSetTint( sceneView.blueMaterial, { 1, 1, 1, 1 } );

    unsigned closestSubMesh = 666;
    int closestSceneGo = -1;

    GetColliders( (unsigned)x, (unsigned)y, false, closestSceneGo, closestSubMesh );

    if (closestSceneGo == (int)sceneView.translateGizmoGo.index)
    {
        if (closestSubMesh == 0)
        {
            teMaterialSetTint( sceneView.greenMaterial, { 2, 2, 2, 1 } );
        }
        if (closestSubMesh == 2)
        {
            teMaterialSetTint( sceneView.redMaterial, { 2, 2, 2, 1 } );
        }
        if (closestSubMesh == 1)
        {
            teMaterialSetTint( sceneView.blueMaterial, { 2, 2, 2, 1 } );
        }
    }

    if (!isLeftMouseDown)
    {
        return;
    }

    if (gizmoAxisSelected == 0)
    {
        teMaterialSetTint( sceneView.greenMaterial, { 2, 2, 2, 1 } );
        teTransformMoveUp( selectedGoIndex, -dy * 0.5f );
        teTransformSetLocalPosition( sceneView.translateGizmoGo.index, teTransformGetLocalPosition( selectedGoIndex ) );
    }
    else if (gizmoAxisSelected == 2)
    {
        teMaterialSetTint( sceneView.redMaterial, { 2, 2, 2, 1 } );
        teTransformMoveRight( selectedGoIndex, dx * 0.5f );
        teTransformSetLocalPosition( sceneView.translateGizmoGo.index, teTransformGetLocalPosition( selectedGoIndex ) );
    }
    else if (gizmoAxisSelected == 1)
    {
        teMaterialSetTint( sceneView.blueMaterial, { 2, 2, 2, 1 } );
        teTransformMoveForward( selectedGoIndex, dx * 0.5f, false, false, false );
        teTransformSetLocalPosition( sceneView.translateGizmoGo.index, teTransformGetLocalPosition( selectedGoIndex ) );
    }
}

void SelectGizmo( unsigned x, unsigned y )
{
    gizmoAxisSelected = -1;
    teMaterialSetTint( sceneView.greenMaterial, { 1, 1, 1, 1 } );
    teMaterialSetTint( sceneView.redMaterial, { 1, 1, 1, 1 } );
    teMaterialSetTint( sceneView.blueMaterial, { 1, 1, 1, 1 } );

    unsigned closestSubMesh = 666;
    int closestSceneGo = -1;

    GetColliders( x, y, false, closestSceneGo, closestSubMesh );

    if (closestSceneGo == (int)sceneView.translateGizmoGo.index)
    {
        gizmoAxisSelected = closestSubMesh;
        printf( "gizmo submesh %d\n", closestSubMesh );
    }

    printf("Gizmo axis selected: %d\n", gizmoAxisSelected );
    //sceneView.selectedGos[ 0 ].index = selectedGoIndex;
}

void DeleteSelectedObject()
{
    if (selectedGoIndex != 1)
    {
        teSceneRemove( sceneView.scene, selectedGoIndex );
    }
    
    selectedGoIndex = 1;
    sceneView.selectedGos[ 0 ].index = selectedGoIndex;
    teMeshRendererSetEnabled( sceneView.translateGizmoGo.index, false );
}

void InitSceneView( unsigned width, unsigned height, void* windowHandle, int uiScale )
{
    teCreateRenderer( 1, windowHandle, width, height );

    sceneView.width = width;
    sceneView.height = height;

    teFile unlitVsFile = teLoadFile( "shaders/unlit_vs.spv" );
    teFile unlitPsFile = teLoadFile( "shaders/unlit_ps.spv" );
    sceneView.unlitShader = teCreateShader( unlitVsFile, unlitPsFile, "unlitVS", "unlitPS" );

    teFile standardVsFile = teLoadFile( "shaders/standard_vs.spv" );
    teFile standardPsFile = teLoadFile( "shaders/standard_ps.spv" );
    sceneView.standardShader = teCreateShader( standardVsFile, standardPsFile, "standardVS", "standardPS" );

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
    Vec3 cameraPos = { 0, 0, 10 };
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
    teMaterialSetTexture2D( sceneView.material, sceneView.gliderTex, 1 );
    
    sceneView.standardMaterial = teCreateMaterial( sceneView.standardShader );
    teMaterialSetTexture2D( sceneView.standardMaterial, sceneView.gliderTex, 0 );

    teFile redFile = teLoadFile( "assets/textures/red.tga" );
    teFile greenFile = teLoadFile( "assets/textures/green.tga" );
    teFile blueFile = teLoadFile( "assets/textures/blue.tga" );

    sceneView.redTex = teLoadTexture( redFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    sceneView.greenTex = teLoadTexture( greenFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    sceneView.blueTex = teLoadTexture( blueFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

    sceneView.redMaterial = teCreateMaterial( sceneView.unlitShader );
    teMaterialSetTexture2D( sceneView.redMaterial, sceneView.redTex, 0 );
    sceneView.greenMaterial = teCreateMaterial( sceneView.unlitShader );
    teMaterialSetTexture2D( sceneView.greenMaterial, sceneView.greenTex, 0 );
    sceneView.blueMaterial = teCreateMaterial( sceneView.unlitShader );
    teMaterialSetTexture2D( sceneView.blueMaterial, sceneView.blueTex, 0 );

    sceneView.cubeMesh = teCreateCubeMesh();
    sceneView.cubeGo = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
    teTransformSetLocalPosition( sceneView.cubeGo.index, Vec3( 0, 0, 0 ) );
    //teTransformSetLocalScale( cubeGo2.index, 2 );
    teMeshRendererSetMesh( sceneView.cubeGo.index, &sceneView.cubeMesh );
    teMeshRendererSetMaterial( sceneView.cubeGo.index, sceneView.standardMaterial, 0 );

    teFile gizmoFile = teLoadFile( "assets/meshes/translation_gizmo.t3d" );
    sceneView.translateGizmoMesh = teLoadMesh( gizmoFile );
    sceneView.translateGizmoGo = teCreateGameObject( "translationGizmo", teComponent::Transform | teComponent::MeshRenderer );
    teTransformSetLocalPosition( sceneView.translateGizmoGo.index, Vec3( 2, 0, 0 ) );
    teTransformSetLocalScale( sceneView.translateGizmoGo.index, 0.25f );
    teMeshRendererSetMesh( sceneView.translateGizmoGo.index, &sceneView.translateGizmoMesh );
    teMeshRendererSetMaterial( sceneView.translateGizmoGo.index, sceneView.greenMaterial, 0 );
    teMeshRendererSetMaterial( sceneView.translateGizmoGo.index, sceneView.blueMaterial, 1 );
    teMeshRendererSetMaterial( sceneView.translateGizmoGo.index, sceneView.redMaterial, 2 );

    sceneView.scene = teCreateScene( 2048 );
    teFinalizeMeshBuffers();

    teSceneAdd( sceneView.scene, sceneView.camera3d.index );
    teSceneAdd( sceneView.scene, sceneView.translateGizmoGo.index );
    teSceneAdd( sceneView.scene, sceneView.cubeGo.index );
    teSceneSetupDirectionalLight( sceneView.scene, Vec3( 1, 1, 1 ), Vec3( 1, 1, 1 ).Normalized() );

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

void RenderSceneView( float gridStep )
{
    teBeginFrame();
    ImGui::NewFrame();
    Vec3 dirLightShadowCasterPosition( 0, 25, 0 );
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
    shaderParams.tint[ 0 ] = 1.0f;
    shaderParams.tint[ 1 ] = 1.0f;
    shaderParams.tint[ 2 ] = 1.0f;
    shaderParams.tint[ 3 ] = 1.0f;
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
        bool selected = false;

        for (unsigned i = 0; i < goCount; ++i)
        {
            unsigned goIndex = teSceneGetGameObjectIndex( sceneView.scene, i );
            
            if (goIndex != 0 && goIndex != sceneView.translateGizmoGo.index && goIndex != sceneView.camera3d.index)
            {
                ImGui::PushID( i );

                if (ImGui::Selectable( teGameObjectGetName( goIndex ), &selected ))
                {
                    selectedGoIndex = goIndex;
                    teMeshRendererSetEnabled( sceneView.translateGizmoGo.index, true );
                    teTransformSetLocalPosition( sceneView.translateGizmoGo.index, teTransformGetLocalPosition( selectedGoIndex ) );
                }

                ImGui::PopID();
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
                const bool positionChanged = ImGui::InputFloat3( "position", (float*)teTransformAccessLocalPosition( selectedGoIndex ), "%.3f", ImGuiInputTextFlags_CharsScientific );
                if (positionChanged)
                {
                    teTransformSetLocalPosition( sceneView.translateGizmoGo.index, teTransformGetLocalPosition( selectedGoIndex ) );
                }
                
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
                            teFile meshFile = teLoadFile( openFilePath );

                            teMesh* mesh = new teMesh; // TODO: better place to store the created meshes than the heap.
                            *mesh = teLoadMesh( meshFile );
                            teFinalizeMeshBuffers();
                            teMeshRendererSetMesh( selectedGoIndex, mesh );
                            
                            for (unsigned i = 0; i < teMeshGetSubMeshCount( mesh ); ++i)
                            {
                                teMeshRendererSetMaterial( selectedGoIndex, sceneView.standardMaterial, i );
                            }
                        }
                    }
                    
                    teMesh* mesh = teMeshRendererGetMesh( selectedGoIndex );
                    
                    for (unsigned i = 0; i < teMeshGetSubMeshCount( mesh ); ++i)
                    {
                        ImGui::Text( "%s", teMeshGetSubMeshName( *mesh, i ) );
                    }
                }
            }
            else if (ImGui::Button( "Add Mesh Renderer" ))
            {
                teGameObjectAddComponent( selectedGoIndex, teComponent::MeshRenderer );
            }
        }
        else
        {
            ImGui::Text( "Sunlight direction" );
            const bool dirChanged = ImGui::InputFloat3( "##dir", lightDir, "%.3f", ImGuiInputTextFlags_CharsScientific | ImGuiInputTextFlags_AllowTabInput );

            if (dirChanged)
            {
                teSceneSetupDirectionalLight( sceneView.scene, Vec3( lightColor[ 0 ], lightColor[ 1 ], lightColor[ 2 ] ), Vec3( lightDir[ 0 ], lightDir[ 1 ], lightDir[ 2 ] ).Normalized() );
            }

            ImGui::Text( "Sunlight color" );
            const bool colorChanged = ImGui::ColorEdit3( "Color", lightColor );

            if (colorChanged)
            {
                teSceneSetupDirectionalLight( sceneView.scene, Vec3( lightColor[ 0 ], lightColor[ 1 ], lightColor[ 2 ] ), Vec3( lightDir[ 0 ], lightDir[ 1 ], lightDir[ 2 ] ).Normalized() );
            }

            ImGui::Text( "Grid: %d", (int)gridStep );
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
    teTransformMoveForward( sceneView.camera3d.index, forward, false, false, false );
    teTransformMoveUp( sceneView.camera3d.index, up );
    teTransformMoveRight( sceneView.camera3d.index, right );
}
