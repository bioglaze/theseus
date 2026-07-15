#include "camera.h"
#include "file.h"
#include "gameobject.h"
#include "material.h"
#include "mesh.h"
#include "renderer.h"
#include "scene.h"
#include "shader.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"
#include "window.h"

double GetMilliseconds();

struct Resources
{
    teShader unlitShader;
    teShader fullscreenShader;
    teScene scene;
    teShader skyboxShader;
    teShader momentsShader;
    teShader depthNormalsShader;
    teShader lightCullShader;
    teTextureCube skyTex;
    teMesh cubeMesh;
    teGameObject camera3d;
} gResources;

struct InputState
{
    int x = 0;
    int y = 0;
    float deltaX = 0;
    float deltaY = 0;
    int lastMouseX = 0;
    int lastMouseY = 0;
    Vec3 moveDir;
} gInput;

struct GameState
{
    double theTime;
    double dt;
} gGameState;

void Init( unsigned width, unsigned height )
{
#if !API_METAL
    void* windowHandle = teCreateWindow( width, height, "Game" );
    teWindowGetSize( width, height );
#else
    void* windowHandle = nullptr;
#endif
    teCreateRenderer( 1, windowHandle, width, height );
    teLoadMetalShaderLibrary();

    teFile unlitVsFile = teLoadFile( "shaders/unlit_vs.spv" );
    teFile unlitPsFile = teLoadFile( "shaders/unlit_ps.spv" );
    gResources.unlitShader = teCreateShader( unlitVsFile, unlitPsFile, "unlitVS", "unlitPS" );

    teFile fullscreenVsFile = teLoadFile( "shaders/fullscreen_vs.spv" );
    teFile fullscreenPsFile = teLoadFile( "shaders/fullscreen_ps.spv" );
    gResources.fullscreenShader = teCreateShader( fullscreenVsFile, fullscreenPsFile, "fullscreenVS", "fullscreenPS" );

    teFile skyboxVsFile = teLoadFile( "shaders/skybox_vs.spv" );
    teFile skyboxPsFile = teLoadFile( "shaders/skybox_ps.spv" );
    gResources.skyboxShader = teCreateShader( skyboxVsFile, skyboxPsFile, "skyboxVS", "skyboxPS" );

    teFile momentsVsFile = teLoadFile( "shaders/moments_vs.spv" );
    teFile momentsPsFile = teLoadFile( "shaders/moments_ps.spv" );
    gResources.momentsShader = teCreateShader( momentsVsFile, momentsPsFile, "momentsVS", "momentsPS" );

    teFile depthNormalsVsFile = teLoadFile( "shaders/depthnormals_vs.spv" );
    teFile depthNormalsPsFile = teLoadFile( "shaders/depthnormals_ps.spv" );
    gResources.depthNormalsShader = teCreateShader( depthNormalsVsFile, depthNormalsPsFile, "depthNormalsVS", "depthNormalsPS" );

    teFile lightCullFile = teLoadFile( "shaders/lightculler.spv" );
    gResources.lightCullShader = teCreateComputeShader( lightCullFile, "cullLights", 8, 8 );

    teFile backFile = teLoadFile( "assets/textures/skybox/back.dds" );
    teFile frontFile = teLoadFile( "assets/textures/skybox/front.dds" );
    teFile leftFile = teLoadFile( "assets/textures/skybox/left.dds" );
    teFile rightFile = teLoadFile( "assets/textures/skybox/right.dds" );
    teFile topFile = teLoadFile( "assets/textures/skybox/top.dds" );
    teFile bottomFile = teLoadFile( "assets/textures/skybox/bottom.dds" );
    gResources.skyTex = teLoadTexture( leftFile, rightFile, bottomFile, topFile, frontFile, backFile, 0 );

    teFile cubeFile = teLoadFile( "assets/meshes/cube.t3d" );
    gResources.cubeMesh = teLoadMesh( cubeFile );

    gResources.scene = teCreateScene( 0 );

    gResources.camera3d = teCreateGameObject( "camera3d", teComponent::Transform | teComponent::Camera );
    Vec3 cameraPos = { 0, 2, 10 };
    Vec4 clearColor = { 1, 0, 0, 1 };
    teClearFlag clearFlag = teClearFlag::DepthAndColor;
    teTransformSetLocalPosition( gResources.camera3d.index, cameraPos );
    teCameraSetProjection( gResources.camera3d.index, 45, width / (float)height, 0.1f, 800.0f );
    teCameraSetClear( gResources.camera3d.index, clearFlag, clearColor );
    teCameraGetColorTexture( gResources.camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::BGRA_sRGB, "camera3d color" );
    teCameraGetDepthTexture( gResources.camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::Depth32F_S8, "camera3d depth" );
    teCameraGetDepthNormalsTexture( gResources.camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::R32G32B32A32F, "camera3d depthNormals" );

    teSceneAdd( gResources.scene, gResources.camera3d.index );

    teSceneSetupDirectionalLight( gResources.scene, Vec3( 1, 1, 1 ), Vec3( 0.005f, -1, 0.005f ).Normalized() );

    teFinalizeMeshBuffers();

    gGameState.theTime = GetMilliseconds();
}

void Render()
{
    double lastTime = gGameState.theTime;
    gGameState.theTime = GetMilliseconds();
    gGameState.dt = gGameState.theTime - lastTime;

    if (gGameState.dt < 0)
    {
        gGameState.dt = 0;
    }

    teBeginFrame();

    Vec3 dirLightShadowCasterPosition;
    teSceneRender( gResources.scene, &gResources.skyboxShader, &gResources.skyTex, &gResources.cubeMesh, gResources.momentsShader, dirLightShadowCasterPosition, gResources.depthNormalsShader, gResources.lightCullShader );

    teBeginSwapchainRendering();

    ShaderParams shaderParams{};
    shaderParams.tilesXY[ 0 ] = 2.0f;
    shaderParams.tilesXY[ 1 ] = 2.0f;
    shaderParams.tilesXY[ 2 ] = -1.0f;
    shaderParams.tilesXY[ 3 ] = -1.0f;
    teDrawQuad( gResources.fullscreenShader, teCameraGetColorTexture( gResources.camera3d.index ), shaderParams, teBlendMode::Off );

    teEndSwapchainRendering();
    
    teEndFrame();
}

void HandleEvent( const teWindowEvent& event )
{
    if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::S)
    {
        gInput.moveDir.z = -0.5f;
    }
    else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::S)
    {
        gInput.moveDir.z = 0;
    }
    else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::W)
    {
        gInput.moveDir.z = 0.5f;
    }
    else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::W)
    {
        gInput.moveDir.z = 0;
    }
    else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::A)
    {
        gInput.moveDir.x = -0.5f;
    }
    else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::A)
    {
        gInput.moveDir.x = 0;
    }
    else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::D)
    {
        gInput.moveDir.x = 0.5f;
    }
    else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::D)
    {
        gInput.moveDir.x = 0;
    }
    else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Q)
    {
        gInput.moveDir.y = -0.5f;
    }

    bool fpsCamera = false;
    const float speed = fpsCamera ? 0.25f : 0.5f;

    teTransformMoveForward( gResources.camera3d.index, gInput.moveDir.z * (float)gGameState.dt * speed, false, fpsCamera, false );
    teTransformMoveRight( gResources.camera3d.index, gInput.moveDir.x * (float)gGameState.dt * speed );
    teTransformMoveUp( gResources.camera3d.index, gInput.moveDir.y * (float)gGameState.dt * speed );
}
