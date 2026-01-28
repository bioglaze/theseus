#include "camera.h"
#include "file.h"
#include "gameobject.h"
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "imgui.h"
#include "light.h"
#include "matrix.h"
#include "mathutil.h"
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
#include <math.h>

float bloomThreshold = 0.1f;

double GetMilliseconds();
void SubmitCommandBuffer();
void BeginCommandBuffer();

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
    teTexture2D textures[ 10 ];
    unsigned textureCount = 0;
};

ImGUIImplCustom impl;

struct FontTextureUpdate
{
    int64_t index = -1;
    unsigned width = 0;
    unsigned height = 0;
    unsigned char* pixels = nullptr;
};

FontTextureUpdate fontTexUpdate;

void RenderImGUIDrawData( const teShader& shader )
{
    ImDrawData* drawData = ImGui::GetDrawData();

    int fbWidth = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
    int fbHeight = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
    // Don't render when minimized.
    if (fbWidth <= 0 || fbHeight <= 0)
        return;

    if (drawData->Textures != nullptr)
    {
        for (ImTextureData* tex : *drawData->Textures)
        {
            if (tex->Status != ImTextureStatus_OK)
            {
                //MyImGuiBackend_UpdateTexture( tex );
                if (tex->Status == ImTextureStatus_WantCreate)
                {
                    printf( "ImTextureStatus_WantCreate\n" );
                    impl.textures[ impl.textureCount ] = teCreateTexture2D( tex->Width, tex->Height, 0, teTextureFormat::RGBA_sRGB, "default" );

                    tex->SetStatus( ImTextureStatus_OK );
                    tex->SetTexID( impl.textureCount + 1 );

                    fontTexUpdate.index = impl.textureCount;
                    fontTexUpdate.width = tex->Width;
                    fontTexUpdate.height = tex->Height;
                    fontTexUpdate.pixels = (unsigned char*)malloc( tex->GetSizeInBytes() );
                    memcpy( fontTexUpdate.pixels, tex->GetPixels(), tex->GetSizeInBytes() );

                    ++impl.textureCount;
                }
                if (tex->Status == ImTextureStatus_WantUpdates)
                {
                    printf( "ImTextureStatus_WantUpdates\n" );
                    /*teFile nullFile;
                    impl.textures[ tex->GetTexID() ] = teLoadTexture( nullFile, 0, tex->GetPixels(), tex->Width, tex->Height, teTextureFormat::RGBA_sRGB );
                    tex->SetStatus( ImTextureStatus_OK );*/
                }
            }
        }
    }

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

                teUIDrawCall( shader, impl.textures[ pcmd->GetTexID() - 1 ], (int)drawData->DisplaySize.x, (int)drawData->DisplaySize.y, (int32_t)clip_min.x, (int32_t)clip_min.y, (uint32_t)(clip_max.x - clip_min.x), (uint32_t)(clip_max.y - clip_min.y), pcmd->ElemCount, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset);
            }
        }

        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }
}

void GetColliders( unsigned screenX, unsigned screenY, unsigned width, unsigned height, teScene scene, unsigned cameraIndex, int& outClosestSceneGo, unsigned& outClosestSubMesh )
{
    Vec3 rayOrigin, rayTarget;
    ScreenPointToRay( screenX, screenY, (float)width, (float)height, cameraIndex, rayOrigin, rayTarget );

    outClosestSceneGo = -1;
    float closestDistance = 99999.0f;
    outClosestSubMesh = 666;

    for (unsigned go = 0; go < teSceneGetMaxGameObjects(); ++go)
    {
        unsigned sceneGo = teSceneGetGameObjectIndex( scene, go );

        if ((teGameObjectGetComponents( sceneGo ) & teComponent::MeshRenderer) == 0)
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

struct InputState
{
    int x = 0;
    int y = 0;
    float deltaX = 0;
    float deltaY = 0;
    int lastMouseX = 0;
    int lastMouseY = 0;
    Vec3 moveDir;
};

teTexture2D key0tex;
teTexture2D key1tex;
teTexture2D key2tex;
teTexture2D key3tex;
teTexture2D key4tex;
teTexture2D key5tex;
teTexture2D key6tex;
teTexture2D key7tex;
teTexture2D key8tex;
teTexture2D key9tex;

teMaterial key0mat;
teMaterial key1mat;
teMaterial key2mat;
teMaterial key3mat;
teMaterial key4mat;
teMaterial key5mat;
teMaterial key6mat;
teMaterial key7mat;
teMaterial key8mat;
teMaterial key9mat;

teMesh keypadMesh;

int main()
{
    unsigned width =  1920 / 1;
    unsigned height =  1080 / 1;
    void* windowHandle = teCreateWindow( width, height, "Theseus Engine Hello" );
    teWindowGetSize( width, height );
    teCreateRenderer( 1, windowHandle, width, height );
    teLoadMetalShaderLibrary();

    teFile unlitMsFile = teLoadFile( "shaders/unlit_ms.spv" );
    teFile unlitVsFile = teLoadFile( "shaders/unlit_vs.spv" );
    teFile unlitPsFile = teLoadFile( "shaders/unlit_ps.spv" );
    teShader unlitShader = teCreateShader( unlitVsFile, unlitPsFile, "unlitVS", "unlitPS" );
    teShader unlitMeshShader = teCreateMeshShader( unlitMsFile, unlitPsFile, "unlitMS", "unlitPS" );

    teFile uiVsFile = teLoadFile( "shaders/ui_vs.spv" );
    teFile uiPsFile = teLoadFile( "shaders/ui_ps.spv" );
    teShader uiShader = teCreateShader( uiVsFile, uiPsFile, "uiVS", "uiPS" );

    teFile fullscreenVsFile = teLoadFile( "shaders/fullscreen_vs.spv" );
    teFile fullscreenPsFile = teLoadFile( "shaders/fullscreen_ps.spv" );
    teFile fullscreenAdditivePsFile = teLoadFile( "shaders/fullscreen_additive_ps.spv" );
    teShader fullscreenShader = teCreateShader( fullscreenVsFile, fullscreenPsFile, "fullscreenVS", "fullscreenPS" );
    teShader fullscreenAdditiveShader = teCreateShader( fullscreenVsFile, fullscreenAdditivePsFile, "fullscreenVS", "fullscreenAdditivePS" );

    teFile skyboxVsFile = teLoadFile( "shaders/skybox_vs.spv" );
    teFile skyboxPsFile = teLoadFile( "shaders/skybox_ps.spv" );
    teShader skyboxShader = teCreateShader( skyboxVsFile, skyboxPsFile, "skyboxVS", "skyboxPS" );

    teFile momentsVsFile = teLoadFile( "shaders/moments_vs.spv" );
    teFile momentsPsFile = teLoadFile( "shaders/moments_ps.spv" );
    teShader momentsShader = teCreateShader( momentsVsFile, momentsPsFile, "momentsVS", "momentsPS" );

    teFile standardVsFile = teLoadFile( "shaders/standard_vs.spv" );
    teFile standardPsFile = teLoadFile( "shaders/standard_ps.spv" );
    teShader standardShader = teCreateShader( standardVsFile, standardPsFile, "standardVS", "standardPS" );

    teFile depthNormalsVsFile = teLoadFile( "shaders/depthnormals_vs.spv" );
    teFile depthNormalsPsFile = teLoadFile( "shaders/depthnormals_ps.spv" );
    teShader depthNormalsShader = teCreateShader( depthNormalsVsFile, depthNormalsPsFile, "depthNormalsVS", "depthNormalsPS" );

    teFile lightCullFile = teLoadFile( "shaders/lightculler.spv" );
    teShader lightCullShader = teCreateComputeShader( lightCullFile, "cullLights", 8, 8 );

    teFile bloomThresholdFile = teLoadFile( "shaders/bloom_threshold.spv" );
    teShader bloomThresholdShader = teCreateComputeShader( bloomThresholdFile, "bloomThreshold", 8, 8 );

    teFile bloomBlurFile = teLoadFile( "shaders/bloom_blur.spv" );
    teShader bloomBlurShader = teCreateComputeShader( bloomBlurFile, "bloomBlur", 8, 8 );

    teFile bloomCombineFile = teLoadFile( "shaders/bloom_combine.spv" );
    teShader bloomCombineShader = teCreateComputeShader( bloomCombineFile, "bloomCombine", 8, 8 );

    teFile downsampleFile = teLoadFile( "shaders/downsample.spv" );
    teShader downsampleShader = teCreateComputeShader( downsampleFile, "bloomDownsample", 8, 8 );

    teGameObject camera3d = teCreateGameObject( "camera3d", teComponent::Transform | teComponent::Camera );
    Vec3 cameraPos = { 0, 2, 10 };
    Vec4 clearColor = { 1, 0, 0, 1 };
    teClearFlag clearFlag = teClearFlag::DepthAndColor;
    teTransformSetLocalPosition( camera3d.index, cameraPos );
    teCameraSetProjection( camera3d.index, 45, width / (float)height, 0.1f, 800.0f );
    teCameraSetClear( camera3d.index, clearFlag, clearColor );
    teCameraGetColorTexture( camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::BGRA_sRGB, "camera3d color" );
    teCameraGetDepthTexture( camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::Depth32F_S8, "camera3d depth" );
    teCameraGetDepthNormalsTexture( camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::R32G32B32A32F, "camera3d depthNormals" );

    teFile gliderFile = teLoadFile( "assets/textures/glider_color.tga" );
    //teFile gliderFile = teLoadFile( "assets/textures/test/nonpow.tga" );
    teTexture2D gliderTex = teLoadTexture( gliderFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    teMaterial materialMS = teCreateMaterial( unlitMeshShader );
    teMaterialSetTexture2D( materialMS, gliderTex, 0 );

    teMaterial standardMaterial = teCreateMaterial( standardShader );
    teMaterialSetTexture2D( standardMaterial, gliderTex, 0 );

    teFile brickFile = teLoadFile( "assets/textures/brickwall_d.dds" );
    teTexture2D brickTex = teLoadTexture( brickFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

    teFile brickNormalFile = teLoadFile( "assets/textures/brickwall_n.dds" );
    teTexture2D brickNormalTex = teLoadTexture( brickNormalFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

    teFile floorFile = teLoadFile( "assets/textures/plaster_d.dds" );
    teTexture2D floorTex = teLoadTexture( floorFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

    teFile floorNormalFile = teLoadFile( "assets/textures/plaster_n.dds" );
    teTexture2D floorNormalTex = teLoadTexture( floorNormalFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

    teMaterial floorMaterial = teCreateMaterial( standardShader );
    teMaterialSetTexture2D( floorMaterial, floorTex, 0 );
    teMaterialSetTexture2D( floorMaterial, floorNormalTex, 1 );

    teMaterial brickMaterial = teCreateMaterial( standardShader );
    teMaterialSetTexture2D( brickMaterial, brickTex, 0 );
    teMaterialSetTexture2D( brickMaterial, brickNormalTex, 1 );

    teFile gridFile = teLoadFile( "assets/textures/metal_grid.dds" );
    teTexture2D gridTex = teLoadTexture( gridFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

    teFile gridNormalFile = teLoadFile( "assets/textures/metal_grid_n.dds" );
    teTexture2D gridNormalTex = teLoadTexture( gridNormalFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

    teMaterial gridMaterial = teCreateMaterial( standardShader );
    teMaterialSetTexture2D( gridMaterial, gridTex, 0 );
    teMaterialSetTexture2D( gridMaterial, gridNormalTex, 1 );

    teFile corridorFile = teLoadFile( "assets/meshes/scifi_corridor.t3d" );
    teMesh corridorMesh = teLoadMesh( corridorFile );
    teGameObject corridorGo = teCreateGameObject( "corridor", teComponent::Transform | teComponent::MeshRenderer );
    printf("materials: %d\n", teMeshGetSubMeshCount( &corridorMesh ) );
    teTransformSetLocalPosition( corridorGo.index, { 0, -10, 0 } );
    teTransformSetLocalScale( corridorGo.index, 0.5f );
    teMeshRendererSetMesh( corridorGo.index, &corridorMesh );
    teMeshRendererSetMaterial( corridorGo.index, gridMaterial, 0 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 1 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 2 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 3 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 4 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 5 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 6 );
    teMeshRendererSetMaterial( corridorGo.index, gridMaterial, 7 );
    teMeshRendererSetMaterial( corridorGo.index, gridMaterial, 8 );
    teMeshRendererSetMaterial( corridorGo.index, gridMaterial, 9 );
    teMeshRendererSetMaterial( corridorGo.index, gridMaterial, 10 );
    teMeshRendererSetMaterial( corridorGo.index, gridMaterial, 11 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 12 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 13 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 14 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 15 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 16 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 17 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 18 );
    teMeshRendererSetMaterial( corridorGo.index, gridMaterial, 19 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 20 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 21 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 22 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 23 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 24 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 25 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 26 );
    teMeshRendererSetMaterial( corridorGo.index, floorMaterial, 27 );

    teMaterialSetTexture2D( standardMaterial, gliderTex, 0 );

    teFile transFile = teLoadFile( "assets/textures/font.tga" );
    teTexture2D transTex = teLoadTexture( transFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    teMaterial materialTransMS = teCreateMaterial( unlitMeshShader );
    materialTransMS.blendMode = teBlendMode::Alpha;
    teMaterialSetTexture2D( materialTransMS, transTex, 0 );

    teFile key0File = teLoadFile( "assets/textures/digits/zero.tga" );
    teFile key1File = teLoadFile( "assets/textures/digits/one.tga" );
    teFile key2File = teLoadFile( "assets/textures/digits/two.tga" );
    teFile key3File = teLoadFile( "assets/textures/digits/three.tga" );
    teFile key4File = teLoadFile( "assets/textures/digits/four.tga" );
    teFile key5File = teLoadFile( "assets/textures/digits/five.tga" );
    teFile key6File = teLoadFile( "assets/textures/digits/six.tga" );
    teFile key7File = teLoadFile( "assets/textures/digits/seven.tga" );
    teFile key8File = teLoadFile( "assets/textures/digits/eight.tga" );
    teFile key9File = teLoadFile( "assets/textures/digits/nine.tga" );
    
    key0tex = teLoadTexture( key0File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    key1tex = teLoadTexture( key1File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    key2tex = teLoadTexture( key2File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    key3tex = teLoadTexture( key3File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    key4tex = teLoadTexture( key4File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    key5tex = teLoadTexture( key5File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    key6tex = teLoadTexture( key6File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    key7tex = teLoadTexture( key7File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    key8tex = teLoadTexture( key8File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    key9tex = teLoadTexture( key9File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

    key0mat = teCreateMaterial( standardShader );
    teMaterialSetTexture2D( key0mat, key0tex, 0 );
    teMaterialSetTexture2D( key0mat, floorNormalTex, 1 );

    key1mat = teCreateMaterial( standardShader );
    teMaterialSetTexture2D( key1mat, key1tex, 0 );
    teMaterialSetTexture2D( key1mat, floorNormalTex, 1 );

    key2mat = teCreateMaterial( standardShader );
    teMaterialSetTexture2D( key2mat, key2tex, 0 );
    teMaterialSetTexture2D( key2mat, floorNormalTex, 1 );

    key3mat = teCreateMaterial( standardShader );
    teMaterialSetTexture2D( key3mat, key3tex, 0 );
    teMaterialSetTexture2D( key3mat, floorNormalTex, 1 );

    key4mat = teCreateMaterial( standardShader );
    teMaterialSetTexture2D( key4mat, key4tex, 0 );
    teMaterialSetTexture2D( key4mat, floorNormalTex, 1 );

    key5mat = teCreateMaterial( standardShader );
    teMaterialSetTexture2D( key5mat, key5tex, 0 );
    teMaterialSetTexture2D( key5mat, floorNormalTex, 1 );

    key6mat = teCreateMaterial( standardShader );
    teMaterialSetTexture2D( key6mat, key6tex, 0 );
    teMaterialSetTexture2D( key6mat, floorNormalTex, 1 );

    key7mat = teCreateMaterial( standardShader );
    teMaterialSetTexture2D( key7mat, key7tex, 0 );
    teMaterialSetTexture2D( key7mat, floorNormalTex, 1 );

    key8mat = teCreateMaterial( standardShader );
    teMaterialSetTexture2D( key8mat, key8tex, 0 );
    teMaterialSetTexture2D( key8mat, floorNormalTex, 1 );

    key9mat = teCreateMaterial( standardShader );
    teMaterialSetTexture2D( key9mat, key9tex, 0 );
    teMaterialSetTexture2D( key9mat, floorNormalTex, 1 );

    teFile keypadFile = teLoadFile( "assets/meshes/keypad.t3d" );
    keypadMesh = teLoadMesh( keypadFile );

    teFile backFile = teLoadFile( "assets/textures/skybox/back.dds" );
    teFile frontFile = teLoadFile( "assets/textures/skybox/front.dds" );
    teFile leftFile = teLoadFile( "assets/textures/skybox/left.dds" );
    teFile rightFile = teLoadFile( "assets/textures/skybox/right.dds" );
    teFile topFile = teLoadFile( "assets/textures/skybox/top.dds" );
    teFile bottomFile = teLoadFile( "assets/textures/skybox/bottom.dds" );
    
    teFile bilinearTestFile = teLoadFile( "assets/textures/test/bilinear_test.tga" );
    teTexture2D bilinearTex = teLoadTexture( bilinearTestFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

    teTextureCube skyTex = teLoadTexture( leftFile, rightFile, bottomFile, topFile, frontFile, backFile, 0 );

    teTexture2D bloomTarget = teCreateTexture2D( width, height, teTextureFlags::UAV, teTextureFormat::R32G32B32A32F, "bloomTarget" );
    teTexture2D blurTarget = teCreateTexture2D( width, height, teTextureFlags::UAV, teTextureFormat::R32G32B32A32F, "blurTarget" );
    teTexture2D bloomComposeTarget = teCreateTexture2D( width, height, teTextureFlags::UAV, teTextureFormat::R32G32B32A32F, "bloomComposeTarget" );
    teTexture2D downsampleTarget = teCreateTexture2D( width / 2, height / 2, teTextureFlags::UAV, teTextureFormat::R32G32B32A32F, "downsampleTarget" );
    teTexture2D downsampleTarget2 = teCreateTexture2D( width / 4, height / 4, teTextureFlags::UAV, teTextureFormat::R32G32B32A32F, "downsampleTarget2" );
    teTexture2D downsampleTarget3 = teCreateTexture2D( width / 8, height / 8, teTextureFlags::UAV, teTextureFormat::R32G32B32A32F, "downsampleTarget3" );

    teTexture2D bilinearTestTarget = teCreateTexture2D( 8, 8, teTextureFlags::UAV, teTextureFormat::R32G32B32A32F, "bilinearTestTarget" );

    teFile bc1File = teLoadFile( "assets/textures/test/test_dxt1.dds" );
    teTexture2D bc1Tex = teLoadTexture( bc1File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
    bc1Tex.sampler = teTextureSampler::NearestRepeat;
    teMaterialSetTexture2D( materialMS, bc1Tex, 0 );

    teFile bc5File = teLoadFile( "assets/textures/test/grass_n_bc5.dds" );
    teTexture2D bc5Tex = teLoadTexture( bc5File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

    teFile roomFile = teLoadFile( "assets/meshes/room.t3d" );
    teMesh roomMesh = teLoadMesh( roomFile );
    teGameObject roomGo = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
    teMeshRendererSetMesh( roomGo.index, &roomMesh );
    teMeshRendererSetMaterial( roomGo.index, floorMaterial, 0 );
    teMeshRendererSetMaterial( roomGo.index, brickMaterial, 1 );
    teMeshRendererSetMaterial( roomGo.index, brickMaterial, 2 );
    teMeshRendererSetMaterial( roomGo.index, brickMaterial, 3 );
    teMeshRendererSetMaterial( roomGo.index, brickMaterial, 4 );
    teMeshRendererSetMaterial( roomGo.index, brickMaterial, 5 );

    teFile cubeFile = teLoadFile( "assets/meshes/cube.t3d" );
    teMesh cubeMesh = teLoadMesh( cubeFile );
    //teMesh cubeMesh = teCreateCubeMesh();
    teGameObject cubeGo = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
    Vec3 cubePos1 = Vec3( 0, 4, 0 );
    teTransformSetLocalPosition( cubeGo.index, cubePos1 );
    teMeshRendererSetMesh( cubeGo.index, &cubeMesh );
    teMeshRendererSetMaterial( cubeGo.index, standardMaterial, 0 );

    //teMesh cubeMesh2 = teCreateCubeMesh();
    teGameObject cubeGo2 = teCreateGameObject( "cube2", teComponent::Transform | teComponent::MeshRenderer );
    Vec3 cubePos = Vec3( 0, 45, 0 );
    teTransformSetLocalPosition( cubeGo2.index, cubePos );
    //teTransformSetLocalScale( cubeGo2.index, 2 );
    teMeshRendererSetMesh( cubeGo2.index, &cubeMesh );
    // FIXME: this currently crashes the GPU, because the mesh shader code is buggy.
    //teMeshRendererSetMaterial( cubeGo2.index, materialTransMS, 0 );
    teMeshRendererSetMaterial( cubeGo2.index, key0mat, 0 );

    teGameObject pointLight = teCreateGameObject( "cube2", teComponent::Transform | teComponent::PointLight );
    teTransformSetLocalPosition( pointLight.index, { 20, 2, 15 } );
    tePointLightSetParams( pointLight.index, 3, { 1, 0, 0 }, 1.0f );
    
    teGameObject pointLight2 = teCreateGameObject( "cube2", teComponent::Transform | teComponent::PointLight );
    teTransformSetLocalPosition( pointLight2.index, { 25, 2, 15 } );
    tePointLightSetParams( pointLight2.index, 3, { 0, 1, 0 }, 1.0f );
    
    teGameObject keypadGo = teCreateGameObject( "keypad", teComponent::Transform | teComponent::MeshRenderer );
    Vec3 keypadPos = Vec3( 20, 4, 15 );
    Quaternion keypadAngle;
    keypadAngle.FromAxisAngle( { 0, 1, 0 }, 180 );
    teTransformSetLocalScale( keypadGo.index, 2 );
    teTransformSetLocalPosition( keypadGo.index, keypadPos );
    teTransformSetLocalRotation( keypadGo.index, keypadAngle );
    teMeshRendererSetMesh( keypadGo.index, &keypadMesh );
    teMeshRendererSetMaterial( keypadGo.index, key0mat, 0 );
    teMeshRendererSetMaterial( keypadGo.index, key0mat, 1 );
    teMeshRendererSetMaterial( keypadGo.index, key0mat, 2 );
    teMeshRendererSetMaterial( keypadGo.index, key0mat, 3 );
    teMeshRendererSetMaterial( keypadGo.index, key0mat, 4 );
    teMeshRendererSetMaterial( keypadGo.index, key1mat, 5 );
    teMeshRendererSetMaterial( keypadGo.index, key2mat, 6 );
    teMeshRendererSetMaterial( keypadGo.index, key3mat, 7 );
    teMeshRendererSetMaterial( keypadGo.index, key4mat, 8 );
    teMeshRendererSetMaterial( keypadGo.index, key5mat, 9 );
    teMeshRendererSetMaterial( keypadGo.index, key6mat, 10 );
    teMeshRendererSetMaterial( keypadGo.index, key7mat, 11 );
    teMeshRendererSetMaterial( keypadGo.index, key8mat, 12 );
    teMeshRendererSetMaterial( keypadGo.index, key9mat, 13 );
    teMeshRendererSetMaterial( keypadGo.index, key0mat, 14 );
    teMeshRendererSetMaterial( keypadGo.index, key0mat, 15 );
    teMeshRendererSetMaterial( keypadGo.index, key0mat, 16 );
    teMeshRendererSetMaterial( keypadGo.index, key0mat, 17 );

    teFile sceneFile = teLoadFile( "assets/hello.tscene" );
    unsigned sceneGoCount = 0;
    unsigned sceneTextureCount = 0;
    unsigned sceneMaterialCount = 0;
    unsigned sceneMeshCount = 0;
    teSceneReadArraySizes( sceneFile, sceneGoCount, sceneTextureCount, sceneMaterialCount, sceneMeshCount );
    teGameObject* sceneGos = (teGameObject*)malloc( sceneGoCount * sizeof( teGameObject ) );
    teTexture2D* sceneTextures = (teTexture2D*)malloc( sceneTextureCount * sizeof( teTexture2D ) );
    teMaterial* sceneMaterials = (teMaterial*)malloc( sceneMaterialCount * sizeof( teMaterial ) );
    teMesh* sceneMeshes = (teMesh*)malloc( sceneMeshCount * sizeof( teMesh ) );
    printf( "gos: %u, textures: %u, materials: %u, meshes: %u\n", sceneGoCount, sceneTextureCount, sceneMaterialCount, sceneMeshCount );
    teSceneReadScene( sceneFile, standardShader, sceneGos, sceneTextures, sceneMaterials, sceneMeshes );

    teScene scene = teCreateScene( 2048 );
    teFinalizeMeshBuffers();

    teSceneAdd( scene, camera3d.index );
    teSceneAdd( scene, cubeGo.index );
    teSceneAdd( scene, cubeGo2.index );
    //teSceneAdd( scene, roomGo.index );
    teSceneAdd( scene, keypadGo.index );
    //teSceneAdd( scene, corridorGo.index );
    teSceneAdd( scene, pointLight.index );
    teSceneAdd( scene, pointLight2.index );

    for (unsigned i = 0; i < sceneGoCount; ++i)
    {
        teSceneAdd( scene, sceneGos[ i ].index );
    }

    teSceneSetupDirectionalLight( scene, Vec3( 1, 1, 1 ), Vec3( 0.005f, -1, 0.005f ).Normalized() );

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
                teMeshRendererSetMaterial( cubes[ g ].index, materialMS, 0 );
                teTransformSetLocalPosition( cubes[ g ].index, Vec3( i * 4.0f - 5.0f, j * 4.0f - 5.0f, -4.0f - k * 4.0f ) );
                //teSceneAdd( scene, cubes[ g ].index );

                float angle = Random100() / 100.0f * 90;
                Vec3 axis{ 1, 1, 1 };
                axis.Normalize();

                rotation.FromAxisAngle( axis, angle );
                teTransformSetLocalRotation( cubes[ g ].index, rotation );
                
                ++g;
            }
        }
    }

    ImGuiContext* imContext = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = (float)width;
    io.DisplaySize.y = (float)height;

    io.BackendRendererUserData = &impl;
    io.BackendRendererName = "imgui_impl_vulkan";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontScaleDpi = 2.0f;

    bool shouldQuit = false;
    bool isRightMouseDown = false;
    bool fpsCamera = false;
    InputState inputParams;

    double theTime = GetMilliseconds();

    ShaderParams shaderParams{};
    shaderParams.tint[ 0 ] = 0.8f;
    shaderParams.tint[ 1 ] = 0.7f;
    shaderParams.tint[ 2 ] = 0.6f;
    shaderParams.tint[ 3 ] = 0.5f;

    unsigned activeDigit = 1; // 1-4

    while (!shouldQuit)
    {
        double lastTime = theTime;
        theTime = GetMilliseconds();
        double dt = theTime - lastTime;

        if (dt < 0)
        {
            dt = 0;
        }
        
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
                inputParams.moveDir.z = -0.5f;
            }
            else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::S)
            {
                inputParams.moveDir.z = 0;
            }
            else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::W)
            {
                inputParams.moveDir.z = 0.5f;
            }
            else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::W)
            {
                inputParams.moveDir.z = 0;
            }
            else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::A)
            {
                inputParams.moveDir.x = -0.5f;
                //io.AddKeyEvent( ImGuiKey_A, true );
            }
            else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::A)
            {
                inputParams.moveDir.x = 0;
            }
            else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::D)
            {
                inputParams.moveDir.x = 0.5f;
            }
            else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::D)
            {
                inputParams.moveDir.x = 0;
            }
            else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Q)
            {
                inputParams.moveDir.y = -0.5f;
            }
            else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::Q)
            {
                inputParams.moveDir.y = 0;
            }
            else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::E)
            {
                inputParams.moveDir.y = 0.5f;
            }
            else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::E)
            {
                inputParams.moveDir.y = 0;
            }
            else if (event.type == teWindowEvent::Type::Mouse1Down)
            {
                inputParams.x = event.x;
                inputParams.y = event.y;
                //isRightMouseDown = true;
                inputParams.lastMouseX = inputParams.x;
                inputParams.lastMouseY = inputParams.y;
                inputParams.deltaX = 0;
                inputParams.deltaY = 0;

                io.AddMouseButtonEvent( 0, true );

                int closestSceneGo = 0;
                unsigned closestSubMesh = 0;
                GetColliders( event.x, event.y, width, height, scene, camera3d.index, closestSceneGo, closestSubMesh );
                //printf( "closest go: %d, closest submesh: %u\n", closestSceneGo, closestSubMesh );
                // display digit submeshes: 1-4
                // number pad: 5-14
                if ((unsigned)closestSceneGo == keypadGo.index)
                {
                    if (closestSubMesh >= 5 && closestSubMesh < 15)
                    {
                        if (closestSubMesh == 5) teMeshRendererSetMaterial( keypadGo.index, key1mat, activeDigit );
                        if (closestSubMesh == 6) teMeshRendererSetMaterial( keypadGo.index, key2mat, activeDigit );
                        if (closestSubMesh == 7) teMeshRendererSetMaterial( keypadGo.index, key3mat, activeDigit );
                        if (closestSubMesh == 8) teMeshRendererSetMaterial( keypadGo.index, key4mat, activeDigit );
                        if (closestSubMesh == 9) teMeshRendererSetMaterial( keypadGo.index, key5mat, activeDigit );
                        if (closestSubMesh == 10) teMeshRendererSetMaterial( keypadGo.index, key6mat, activeDigit );
                        if (closestSubMesh == 11) teMeshRendererSetMaterial( keypadGo.index, key7mat, activeDigit );
                        if (closestSubMesh == 12) teMeshRendererSetMaterial( keypadGo.index, key8mat, activeDigit );
                        if (closestSubMesh == 13) teMeshRendererSetMaterial( keypadGo.index, key9mat, activeDigit );
                        if (closestSubMesh == 14) teMeshRendererSetMaterial( keypadGo.index, key0mat, activeDigit );

                        ++activeDigit;
                        if (activeDigit == 5)
                        {
                            activeDigit = 1;
                        }
                    }
                }
            }
            else if (event.type == teWindowEvent::Type::Mouse1Up)
            {
                inputParams.x = event.x;
                inputParams.y = event.y;
                //isRightMouseDown = false;
                inputParams.deltaX = 0;
                inputParams.deltaY = 0;
                inputParams.lastMouseX = inputParams.x;
                inputParams.lastMouseY = inputParams.y;

                io.AddMouseButtonEvent( 0, false );
            }
            else if (event.type == teWindowEvent::Type::Mouse2Down)
            {
                inputParams.x = event.x;
                inputParams.y = event.y;
                isRightMouseDown = true;
                inputParams.lastMouseX = inputParams.x;
                inputParams.lastMouseY = inputParams.y;
                inputParams.deltaX = 0;
                inputParams.deltaY = 0;
            }
            else if (event.type == teWindowEvent::Type::Mouse2Up)
            {
                inputParams.x = event.x;
                inputParams.y = event.y;
                isRightMouseDown = false;
                inputParams.deltaX = 0;
                inputParams.deltaY = 0;
                inputParams.lastMouseX = inputParams.x;
                inputParams.lastMouseY = inputParams.y;
            }
            else if (event.type == teWindowEvent::Type::MouseMove)
            {
                inputParams.x = event.x;
                inputParams.y = event.y;
                inputParams.deltaX = float( inputParams.x - inputParams.lastMouseX );
                inputParams.deltaY = float( inputParams.y - inputParams.lastMouseY );
                inputParams.lastMouseX = inputParams.x;
                inputParams.lastMouseY = inputParams.y;

                if (isRightMouseDown)
                {
                    teTransformOffsetRotate( camera3d.index, Vec3( 0, 1, 0 ), -inputParams.deltaX / 100.0f * (float)dt );
                    teTransformOffsetRotate( camera3d.index, Vec3( 1, 0, 0 ), -inputParams.deltaY / 100.0f * (float)dt );
                }

                io.AddMousePosEvent( (float)inputParams.x, (float)inputParams.y );
            }
        }

        Vec3 oldCameraPos = teTransformGetLocalPosition( camera3d.index );

        const float speed = fpsCamera ? 0.25f : 0.5f;

        teTransformMoveForward( camera3d.index, inputParams.moveDir.z * (float)dt * speed, false, fpsCamera, false );
        teTransformMoveRight( camera3d.index, inputParams.moveDir.x * (float)dt * speed );
        teTransformMoveUp( camera3d.index, inputParams.moveDir.y * (float)dt * speed );

        cameraPos = teTransformGetLocalPosition(camera3d.index);
        bool collisionDetection = false;

        if (collisionDetection && teScenePointInsideAABB( scene, cameraPos ))
        {
            teTransformSetLocalPosition( camera3d.index, oldCameraPos );
            teTransformMoveForward( camera3d.index, inputParams.moveDir.z * (float)dt* speed, false, fpsCamera, true );
            cameraPos = teTransformGetLocalPosition( camera3d.index );

            if (teScenePointInsideAABB( scene, cameraPos ))
            {
                teTransformSetLocalPosition(camera3d.index, oldCameraPos);
                teTransformMoveForward( camera3d.index, inputParams.moveDir.z* (float)dt* speed, true, fpsCamera, false );
                cameraPos = teTransformGetLocalPosition( camera3d.index );
                if (teScenePointInsideAABB( scene, cameraPos ))
                {
                    teTransformSetLocalPosition( camera3d.index, oldCameraPos );
                }
            }
            else
            {
            }
        }

        teBeginFrame();

        ImGui::NewFrame();
        const Vec3 dirLightShadowCasterPosition = cubePos;
        teSceneRender( scene, &skyboxShader, &skyTex, &cubeMesh, momentsShader, dirLightShadowCasterPosition, depthNormalsShader, lightCullShader );

        shaderParams.readTexture = bilinearTex.index;
        shaderParams.writeTexture = bilinearTestTarget.index;
        shaderParams.tilesXY[ 0 ] = 16 / 2;
        shaderParams.tilesXY[ 1 ] = 16 / 2;
        teShaderDispatch( downsampleShader, 16 / 8, 16 / 8, 1, shaderParams, "bilinearTest" );

#if 1
        unsigned dh = (height + 7) / 8;

        shaderParams.readTexture = teCameraGetColorTexture( camera3d.index ).index;
        shaderParams.writeTexture = bloomTarget.index;
        shaderParams.bloomThreshold = (float)pow( 2, bloomThreshold ) - 1;
        teShaderDispatch( bloomThresholdShader, width / 8, dh, 1, shaderParams, "bloom threshold" );
        
        // TODO UAV barrier here
        shaderParams.readTexture = bloomTarget.index;
        shaderParams.writeTexture = blurTarget.index;
        shaderParams.tilesXY[ 2 ] = 1.0f;
        shaderParams.tilesXY[ 3 ] = 0.0f;
        teShaderDispatch( bloomBlurShader, width / 8, dh, 1, shaderParams, "bloom blur h" );

        // TODO UAV barrier here
        shaderParams.readTexture = blurTarget.index;
        shaderParams.writeTexture = bloomTarget.index;
        shaderParams.tilesXY[ 2 ] = 0.0f;
        shaderParams.tilesXY[ 3 ] = 1.0f;
        teShaderDispatch( bloomBlurShader, width / 8, dh, 1, shaderParams, "bloom blur v" );

        // additional blurring
        shaderParams.readTexture = bloomTarget.index;
        shaderParams.writeTexture = blurTarget.index;
        shaderParams.tilesXY[ 2 ] = 1.0f;
        shaderParams.tilesXY[ 3 ] = 0.0f;
        teShaderDispatch( bloomBlurShader, width / 8, dh, 1, shaderParams, "bloom blur h 2" );

        shaderParams.readTexture = blurTarget.index;
        shaderParams.writeTexture = bloomTarget.index;
        shaderParams.tilesXY[ 2 ] = 0.0f;
        shaderParams.tilesXY[ 3 ] = 1.0f;
        teShaderDispatch( bloomBlurShader, width / 8, dh, 1, shaderParams, "bloom blur v 2" );

        // additional blurring 2
        shaderParams.readTexture = bloomTarget.index;
        shaderParams.writeTexture = blurTarget.index;
        shaderParams.tilesXY[ 2 ] = 1.0f;
        shaderParams.tilesXY[ 3 ] = 0.0f;
        teShaderDispatch( bloomBlurShader, width / 8, dh, 1, shaderParams, "bloom blur h 3" );

        shaderParams.readTexture = blurTarget.index;
        shaderParams.writeTexture = bloomTarget.index;
        shaderParams.tilesXY[ 2 ] = 0.0f;
        shaderParams.tilesXY[ 3 ] = 1.0f;
        teShaderDispatch( bloomBlurShader, width / 8, dh, 1, shaderParams, "bloom blur v 3" );

        // Downsample init.
        shaderParams.tilesXY[ 2 ] = 1.0f;
        shaderParams.tilesXY[ 3 ] = 1.0f;

        // Downsample 1
        shaderParams.readTexture = bloomTarget.index;
        shaderParams.writeTexture = downsampleTarget.index;
        shaderParams.tilesXY[ 0 ] = width / 2;
        shaderParams.tilesXY[ 1 ] = height / 2;
        teShaderDispatch( downsampleShader, width / 8, dh, 1, shaderParams, "bloom downsample 1" );

        // Downsample 2
        shaderParams.readTexture = downsampleTarget.index;
        shaderParams.writeTexture = downsampleTarget2.index;
        shaderParams.tilesXY[ 0 ] = width / 4;
        shaderParams.tilesXY[ 1 ] = height / 4;
        teShaderDispatch( downsampleShader, width / 8, dh, 1, shaderParams, "bloom downsample 2" );

        // Downsample 3
        shaderParams.readTexture = downsampleTarget2.index;
        shaderParams.writeTexture = downsampleTarget3.index;
        shaderParams.tilesXY[ 0 ] = width / 8;
        shaderParams.tilesXY[ 1 ] = height / 8;
        teShaderDispatch( downsampleShader, width / 8, dh, 1, shaderParams, "bloom downsample 3" );

        // Combine
        shaderParams.readTexture = bloomTarget.index;
        shaderParams.readTexture2 = downsampleTarget.index;
        shaderParams.readTexture3 = downsampleTarget2.index;
        shaderParams.readTexture4 = downsampleTarget3.index;
        shaderParams.writeTexture = bloomComposeTarget.index;
        shaderParams.tilesXY[ 0 ] = width / 2;
        shaderParams.tilesXY[ 1 ] = height / 2;
        teShaderDispatch( bloomCombineShader, width / 8, dh, 1, shaderParams, "bloom combine" );
#endif

        teBeginSwapchainRendering();
        shaderParams.tilesXY[ 0 ] = 2.0f;
        shaderParams.tilesXY[ 1 ] = 2.0f;
        shaderParams.tilesXY[ 2 ] = -1.0f;
        shaderParams.tilesXY[ 3 ] = -1.0f;
        teDrawQuad( fullscreenShader, teCameraGetColorTexture( camera3d.index ), shaderParams, teBlendMode::Off );

        shaderParams.tilesXY[ 0 ] = 4.0f;
        shaderParams.tilesXY[ 1 ] = 4.0f;
        //teDrawQuad( fullscreenAdditiveShader, /*bilinearTestTarget*/bloomComposeTarget, shaderParams, teBlendMode::Additive);

        ImGui::Begin( "Info" );
        ImGui::Text( "draw calls: %.0f\nPSO binds: %.0f", teRendererGetStat( teStat::DrawCalls ), teRendererGetStat( teStat::PSOBinds ) );
        ImGui::SliderFloat( "Bloom Threshold", &bloomThreshold, 0.01f, 1.0f );
        ImGui::SliderFloat( "Compose Weight 0", &shaderParams.tint[ 0 ], 0.01f, 1.0f );
        ImGui::SliderFloat( "Compose Weight 1", &shaderParams.tint[ 1 ], 0.01f, 1.0f );
        ImGui::SliderFloat( "Compose Weight 2", &shaderParams.tint[ 2 ], 0.01f, 1.0f );
        ImGui::SliderFloat( "Compose Weight 3", &shaderParams.tint[ 3 ], 0.01f, 1.0f );
        ImGui::End();
        ImGui::Render();

        RenderImGUIDrawData( uiShader );

        teEndSwapchainRendering();
        
        if (fontTexUpdate.index != -1)
        {
            SubmitCommandBuffer();

            teFile nullFile2;
            memcpy( nullFile2.path, "font texture", strlen( "font texture" ) );
            impl.textures[ fontTexUpdate.index ] = teLoadTexture( nullFile2, 0, fontTexUpdate.pixels, fontTexUpdate.width, fontTexUpdate.height, teTextureFormat::RGBA_sRGB );
            fontTexUpdate.index = -1;
            BeginCommandBuffer();
        }

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
    free( bloomThresholdFile.data );
    free( gliderFile.data );
    free( backFile.data );
    free( frontFile.data );
    free( leftFile.data );
    free( rightFile.data );
    free( topFile.data );
    free( bottomFile.data );
    free( bc1File.data );

    io.BackendRendererUserData = nullptr;
    ImGui::DestroyContext( imContext );

    return 0;
}
