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
void teGetCorners( const Vec3& min, const Vec3& max, Vec3 outCorners[ 8 ] );

constexpr unsigned MaxSelectedObjects = 10;

char text[ 100 ];
char openFilePath[ 280 ];
unsigned selectedGoIndex = 1; // 1 is editor camera which the UI considers as non-selection.
float pos[ 3 ];
float scale = 1;

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
    teTexture2D redTex;
    teTexture2D greenTex;
    teTexture2D blueTex;

    teMesh cubeMesh;
    teMesh translateGizmoMesh;
    teMaterial material;
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

static void GetMinMax( const Vec3* points, int count, Vec3& outMin, Vec3& outMax )
{
    outMin = points[ 0 ];
    outMax = points[ 0 ];

    for (int i = 1, s = count; i < s; ++i)
    {
        const Vec3& point = points[ i ];

        if (point.x < outMin.x)
        {
            outMin.x = point.x;
        }

        if (point.y < outMin.y)
        {
            outMin.y = point.y;
        }

        if (point.z < outMin.z)
        {
            outMin.z = point.z;
        }

        if (point.x > outMax.x)
        {
            outMax.x = point.x;
        }

        if (point.y > outMax.y)
        {
            outMax.y = point.y;
        }

        if (point.z > outMax.z)
        {
            outMax.z = point.z;
        }
    }
}

static float Min2( float a, float b )
{
    return a < b ? a : b;
}

static float Max2( float a, float b )
{
    return a < b ? b : a;
}

float IntersectRayAABB( const Vec3& origin, const Vec3& target, const Vec3& min, const Vec3& max )
{
    const Vec3 dir = (origin - target).Normalized(); // NOTE: This was "target - origin" in Aether3D Editor.
    const Vec3 dirfrac{ 1.0f / dir.x, 1.0f / dir.y, 1.0f / dir.z };

    const float t1 = (min.x - origin.x) * dirfrac.x;
    const float t2 = (max.x - origin.x) * dirfrac.x;
    const float t3 = (min.y - origin.y) * dirfrac.y;
    const float t4 = (max.y - origin.y) * dirfrac.y;
    const float t5 = (min.z - origin.z) * dirfrac.z;
    const float t6 = (max.z - origin.z) * dirfrac.z;

    const float tmin = Max2( Max2( Min2( t1, t2 ), Min2( t3, t4 ) ), Min2( t5, t6 ) );
    const float tmax = Min2( Min2( Max2( t1, t2 ), Max2( t3, t4 ) ), Max2( t5, t6 ) );

    // if tmax < 0, ray (line) is intersecting AABB, but whole AABB is behing us
    if (tmax < 0)
    {
        return -1.0f;
    }

    // if tmin > tmax, ray doesn't intersect AABB
    if (tmin > tmax)
    {
        return -1.0f;
    }

    return tmin;
}

void GetColliders( unsigned screenX, unsigned screenY )
{
    Vec3 rayOrigin, rayTarget;
    ScreenPointToRay( screenX, screenY, (float)sceneView.width, (float)sceneView.height, sceneView.camera3d, rayOrigin, rayTarget );

    int closestSceneGo = -1;
    float closestDistance = 99999.0f;

    for (unsigned go = 0; go < teSceneGetMaxGameObjects(); ++go)
    {
        unsigned sceneGo = teSceneGetGameObjectIndex( sceneView.scene, go );

        if ((teGameObjectGetComponents( sceneGo ) & teComponent::MeshRenderer) == 0)
        {
            continue;
        }

        for (unsigned subMesh = 0; subMesh < teMeshGetSubMeshCount( teMeshRendererGetMesh( sceneGo ) ); ++subMesh)
        {
            Vec3 mMinLocal, mMaxLocal;
            Vec3 mMinWorld, mMaxWorld;
            Vec3 mAABB[ 8 ];

            teMeshGetSubMeshLocalAABB( teMeshRendererGetMesh( sceneGo ), subMesh, mMinLocal, mMaxLocal );
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
                closestSceneGo = sceneGo;
            }
        }
    }

    if (closestSceneGo != -1)
    {
        selectedGoIndex = closestSceneGo;

        teTransformSetLocalPosition( sceneView.translateGizmoGo.index, teTransformGetLocalPosition( selectedGoIndex ) );
    }
    
    teMeshRendererSetEnabled( sceneView.translateGizmoGo.index, closestSceneGo != -1 );
}

void SelectObject( unsigned x, unsigned y )
{
    selectedGoIndex = 1;
    GetColliders( x, y );
    sceneView.selectedGos[ 0 ].index = selectedGoIndex;
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
    teMeshRendererSetMaterial( sceneView.cubeGo.index, sceneView.material, 0 );

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
                    teMeshRendererSetEnabled( sceneView.translateGizmoGo.index, true );
                    teTransformSetLocalPosition( sceneView.translateGizmoGo.index, teTransformGetLocalPosition( selectedGoIndex ) );
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
                            //teMeshRendererSetMesh( selectedGoIndex, mesh );
                        }
                    }
                    
                    teMesh& mesh = teMeshRendererGetMesh( selectedGoIndex );
                    
                    for (int i = 0; i < teMeshGetSubMeshCount( mesh ); ++i)
                    {
                        ImGui::Text( "submesh" );
                    }
                }
            }
            else if (ImGui::Button( "Add Mesh Renderer" ))
            {
                printf( "TODO: add mesh renderer\n" );
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
