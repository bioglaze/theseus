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
#include <stdint.h>

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

int main()
{
    constexpr unsigned width = 1920;
    constexpr unsigned height = 1080;
    void* windowHandle = teCreateWindow( width, height, "Theseus Engine Hello" );
    teCreateRenderer( 1, windowHandle, width, height );

    teFile unlitVsFile = teLoadFile( "shaders/unlit_vs.spv" );
    teFile unlitPsFile = teLoadFile( "shaders/unlit_ps.spv" );
    teShader unlitShader = teCreateShader( unlitVsFile, unlitPsFile, "unlitVS", "unlitPS" );

    teFile fullscreenVsFile = teLoadFile( "shaders/fullscreen_vs.spv" );
    teFile fullscreenPsFile = teLoadFile( "shaders/fullscreen_ps.spv" );
    teShader fullscreenShader = teCreateShader( fullscreenVsFile, fullscreenPsFile, "fullscreenVS", "fullscreenPS" );

    teGameObject camera3d = teCreateGameObject( "camera3d", teComponent::Transform | teComponent::Camera );
    Vec3 cameraPos = { 0, 0, -10 };
    teTransformSetLocalPosition( camera3d.index, cameraPos );
    teCameraSetProjection( camera3d.index, 45, width / (float)height, 0.1f, 800.0f );

    teCameraGetColorTexture( camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::BGRA_sRGB, "camera3d color" );
    teCameraGetDepthTexture( camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::Depth32F, "camera3d depth" );

    teFile gliderFile = teLoadFile( "assets/textures/glider_color.tga" );
    teTexture2D gliderTex = teLoadTexture( gliderFile, 0 );

    teMaterial material = teCreateMaterial( unlitShader );
    //teMaterialSetParams( teDepthMode::LessOrEqualWriteOff, teBlendMode::Off, teCullMode::CCW );
    teMaterialSetTexture2D( material, gliderTex, 0 );

    teFile backFile = teLoadFile( "assets/textures/skybox/back.dds" );
    teFile frontFile = teLoadFile( "assets/textures/skybox/front.dds" );
    teFile leftFile = teLoadFile( "assets/textures/skybox/left.dds" );
    teFile rightFile = teLoadFile( "assets/textures/skybox/right.dds" );
    teFile topFile = teLoadFile( "assets/textures/skybox/top.dds" );
    teFile bottomFile = teLoadFile( "assets/textures/skybox/bottom.dds" );

    teTextureCube cubeTex = teLoadTexture( leftFile, rightFile, bottomFile, topFile, frontFile, backFile, teTextureFlags::SRGB, teTextureFilter::LinearRepeat );

    teMesh cubeMesh = teCreateCubeMesh();
    teGameObject cubeGo = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
    teMeshRendererSetMesh( cubeGo.index, &cubeMesh );
    teMeshRendererSetMaterial( cubeGo.index, material, 0 );

    teFinalizeMeshBuffers();

    teScene scene = teCreateScene();
    teSceneAdd( scene, camera3d.index );
    //teSceneAdd( scene, cubeGo.index );

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
                teTransformSetLocalPosition( cubes[ g ].index, Vec3( i * 4 - 5, j * 4 - 5, -4 - k * 4 ) );
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

    ImGuiContext* imContext = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = width;
    io.DisplaySize.y = height;
    ImGui::StyleColorsDark();
    unsigned char* fontPixels;
    int fontWidth, fontHeight;
    io.Fonts->GetTexDataAsRGBA32( &fontPixels, &fontWidth, &fontHeight );

    bool shouldQuit = false;
    bool isRightMouseDown = false;
    int x = 0;
    int y = 0;
    float deltaX = 0;
    float deltaY = 0;
    int lastMouseX = 0;
    int lastMouseY = 0;
    Vec3 moveDir;

    while (!shouldQuit)
    {
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

            if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::S)
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
                    teTransformOffsetRotate( camera3d.index, Vec3( 0, 1, 0 ), -deltaX / 100.0f );
                    teTransformOffsetRotate( camera3d.index, Vec3( 1, 0, 0 ), -deltaY / 100.0f );
                }
            }
        }

        teTransformMoveForward( camera3d.index, moveDir.z );
        teTransformMoveRight( camera3d.index, moveDir.x );
        teTransformMoveUp( camera3d.index, moveDir.y );

        teBeginFrame();
        ImGui::NewFrame();
        teSceneRender( scene );

        ImGui::Begin( "ImGUI" );
        ImGui::Text( "This is some useful text." );
        ImGui::End();
        ImGui::Render();

        teBeginSwapchainRendering( teCameraGetColorTexture( camera3d.index ) );
        teDrawFullscreenTriangle( fullscreenShader, teCameraGetColorTexture( camera3d.index ) );
        teEndSwapchainRendering();

        teEndFrame();
    }

    delete[] unlitVsFile.data;
    delete[] unlitPsFile.data;
    delete[] fullscreenVsFile.data;
    delete[] fullscreenPsFile.data;
    delete[] gliderFile.data;
    delete[] backFile.data;
    delete[] frontFile.data;
    delete[] leftFile.data;
    delete[] rightFile.data;
    delete[] topFile.data;
    delete[] bottomFile.data;

    ImGui::DestroyContext( imContext );

    return 0;
}
