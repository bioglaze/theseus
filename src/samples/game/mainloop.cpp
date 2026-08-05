#include "audio.h"
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

#include "game.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

double GetMilliseconds();

struct Resources
{
    static constexpr unsigned MaxSceneMeshes = 30;

    teShader unlitShader;
    teShader fullscreenShader;
    teScene scene;
    teShader skyboxShader;
    teShader momentsShader;
    teShader depthNormalsShader;
    teShader lightCullShader;
    teShader standardShader;
    teTextureCube skyTex;
    teTexture2D defaultTexture2D;
    teMesh cubeMesh;
    teGameObject camera3d;
    teMaterial defaultMaterial;
    teMesh sceneMeshes[ MaxSceneMeshes ];
    teAudioClip audioClip1;
    teAudioClip audioClip2;
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
    bool isRightMouseDown = false;
} gInput;

struct GameState
{
    double theTime;
    double dt;
} gGameState;

void ZeroMem( char* dst, size_t size )
{
    for (unsigned i = 0; i < size; ++i)
    {
        dst[ i ] = 0;
    }
}

void GameSceneReadArraySizes( const teFile& sceneFile, unsigned& outGoCount )
{
    outGoCount = 0;

    char line[ 255 ] = {};
    unsigned cursor = 0;
    unsigned i = 0;

    while (cursor < sceneFile.size)
    {
        line[ i ] = sceneFile.data[ cursor ];
        ++i;

        if (sceneFile.data[ cursor ] == '\n')
        {
            line[ i - 1 ] = 0;
            i = 0;
            if (strstr( line, "gameobject" ) == line)
            {
                ++outGoCount;
            }
            else if (strstr( line, "meshrenderer" ) == line)
            {
            }
            ZeroMem( line, 255 );
        }

        ++cursor;
    }
}

void GameSceneReadScene( const teFile& sceneFile, teGameObject* gos )
{
    unsigned goCount = 0;

    char line[ 255 ] = {};
    unsigned cursor = 0;
    unsigned i = 0;

    while (cursor < sceneFile.size)
    {
        line[ i ] = sceneFile.data[ cursor ];
        ++i;

        if (sceneFile.data[ cursor ] == '\n')
        {
            line[ i - 1 ] = 0;
            i = 0;

            if (strstr( line, "gameobject" ) == line)
            {
                char name[ 100 ] = {};
                unsigned nameCursor = 0;
                unsigned offset = (unsigned)strlen( "gameobject " );

                while (nameCursor + offset < strlen( line ) &&
                    line[ nameCursor + offset ] != '\r' && line[ nameCursor + offset ] != '\n')
                {
                    name[ nameCursor ] = line[ nameCursor + offset ];
                    ++nameCursor;
                }
                printf( "gameobject name: %s\n", name );
                gos[ goCount ] = teCreateGameObject( "gameobject", teComponent::Transform );
                teGameObjectSetName( gos[ goCount ].index, name );
                ++goCount;
            }
            else if (strstr( line, "entity" ) == line)
            {
                char name[ 100 ] = {};
                unsigned nameCursor = 0;
                unsigned offset = (unsigned)strlen( "entity " );

                while (nameCursor + offset < strlen( line ) &&
                    line[ nameCursor + offset ] != '\r' && line[ nameCursor + offset ] != '\n')
                {
                    name[ nameCursor ] = line[ nameCursor + offset ];
                    ++nameCursor;
                }
                
                if (strcmp( name, "start" ) == 0)
                {
                    teTransformSetLocalPosition( gResources.camera3d.index, teTransformGetLocalPosition( gos[ goCount - 1 ].index ) );
                }
            }
            else if (strstr( line, "position" ) == line)
            {
                char position[ 100 ] = {};
                unsigned positionCursor = 0;
                unsigned offset = (unsigned)strlen( "position " );

                while (positionCursor + offset < strlen( line ) &&
                    line[ positionCursor + offset ] != ' ')
                {
                    position[ positionCursor ] = line[ positionCursor + offset ];
                    ++positionCursor;
                }

                float x = (float)atof( position );
                offset += positionCursor+1;
                positionCursor = 0;

                while (positionCursor + offset < strlen( line ) &&
                    line[ positionCursor + offset ] != ' ')
                {
                    position[ positionCursor ] = line[ positionCursor + offset ];
                    ++positionCursor;
                }
                
                float y = atof( position );
                offset += positionCursor;
                positionCursor = 0;

                while (positionCursor + offset < strlen( line ) &&
                    line[ positionCursor + offset ] != '\r' && line[ positionCursor + offset ] != '\n')
                {
                    position[ positionCursor ] = line[ positionCursor + offset ];
                    ++positionCursor;
                }

                float z = (float)atof( position );

                //printf( "position: %f %f %f\n", x, y, z );
                teTransformSetLocalPosition( gos[ goCount - 1 ].index, Vec3( x, y, z ) );
            }
            else if (strstr( line, "meshrenderer" ) == line)
            {
                char name[ 100 ] = {};
                unsigned nameCursor = 0;
                unsigned offset = (unsigned)strlen( "meshrenderer " );

                while (nameCursor + offset < strlen( line ) &&
                    line[ nameCursor + offset ] != '\r' && line[ nameCursor + offset ] != '\n')
                {
                    name[ nameCursor ] = line[ nameCursor + offset ];
                    ++nameCursor;
                }
                
                teGameObjectAddComponent( gos[ goCount - 1 ].index, teComponent::MeshRenderer );

                bool found = false;
                unsigned meshIndex = 0;

                for (unsigned m = 0; m < Resources::MaxSceneMeshes; ++m)
                {
                    if (strcmp( gResources.sceneMeshes[ m ].path, name ) == 0)
                    {
                        found = true;
                        meshIndex = m;
                        break;
                    }
                }

                if (!found)
                {
                    unsigned freeIndex = 0;

                    for (unsigned m = 0; m < Resources::MaxSceneMeshes; ++m)
                    {
                        if (gResources.sceneMeshes[ m ].path[ 0 ] == 0)
                        {
                            break;
                        }

                        ++freeIndex;
                    }
                    teFile meshFile = teLoadFile( name );
                    gResources.sceneMeshes[ freeIndex ] = teLoadMesh( meshFile );
                }

                teMeshRendererSetMesh( gos[ goCount - 1 ].index, &gResources.sceneMeshes[ meshIndex ] );

                for (unsigned m = 0; m < teMeshGetSubMeshCount( &gResources.sceneMeshes[ meshIndex ] ); ++m)
                {
                    teMeshRendererSetMaterial( gos[ goCount - 1 ].index, gResources.defaultMaterial, m );
                }
            }
        }

        ++cursor;
    }
}

void LoadResources( unsigned width, unsigned height )
{
    teFile standardVsFile = teLoadFile( "shaders/standard_vs.spv" );
    teFile standardPsFile = teLoadFile( "shaders/standard_ps.spv" );
    gResources.standardShader = teCreateShader( standardVsFile, standardPsFile, "standardVS", "standardPS" );

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

    //teFile brickFile = teLoadFile( "assets/textures/brickwall_d.dds" );
    teFile brickFile = teLoadFile( "assets/textures/test/manhole_diamond_bc4_with_mips.dds" );
    gResources.defaultTexture2D = teLoadTexture( brickFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

    teFile cubeFile = teLoadFile( "assets/meshes/cube.t3d" );
    gResources.cubeMesh = teLoadMesh( cubeFile );

    gResources.scene = teCreateScene( 0 );

    gResources.defaultMaterial = teCreateMaterial( gResources.standardShader );
    teMaterialSetTexture2D( gResources.defaultMaterial, gResources.defaultTexture2D, 0 );

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

    teFile sceneFile = teLoadFile( "game_proto.tscene" );
    unsigned goCount = 0;
    GameSceneReadArraySizes( sceneFile, goCount );
    teGameObject* sceneGos = (teGameObject*)malloc( goCount * sizeof( teGameObject ) );
    GameSceneReadScene( sceneFile, sceneGos );

    for (unsigned i = 0; i < goCount; ++i)
    {
        teSceneAdd( gResources.scene, sceneGos[ i ].index );
    }

    teSceneSetupDirectionalLight( gResources.scene, Vec3( 1, 1, 1 ), Vec3( 0.005f, -1, 0.005f ).Normalized() );

    teFinalizeMeshBuffers();

    teFile wavFile1 = teLoadFile( "assets/audio/alarm.wav" );
    gResources.audioClip1 = teLoadAudioClip( wavFile1 );

    //teFile wavFile2 = teLoadFile( "assets/audio/sine340.wav" );
    //gResources.audioClip2 = teLoadAudioClip( wavFile2 );

    tePlayAudioClip( gResources.audioClip1 );
}

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

    InitAudio();

    LoadResources( width, height );

    gGameState.theTime = GetMilliseconds();
}

void Tick()
{
    bool fpsCamera = true;
    const float speed = fpsCamera ? 0.25f : 0.5f;

    teTransformMoveForward( gResources.camera3d.index, gInput.moveDir.z * (float)gGameState.dt * speed, false, fpsCamera, false );
    teTransformMoveRight( gResources.camera3d.index, gInput.moveDir.x * (float)gGameState.dt * speed );
    teTransformMoveUp( gResources.camera3d.index, gInput.moveDir.y * (float)gGameState.dt * speed );

    double lastTime = gGameState.theTime;
    gGameState.theTime = GetMilliseconds();
    gGameState.dt = gGameState.theTime - lastTime;

    if (gGameState.dt < 0)
    {
        gGameState.dt = 0;
    }
}

void Render()
{
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

    if (event.type == teWindowEvent::Type::Mouse2Down)
    {
        gInput.x = event.x;
        gInput.y = event.y;
        gInput.isRightMouseDown = true;
        gInput.lastMouseX = gInput.x;
        gInput.lastMouseY = gInput.y;
        gInput.deltaX = 0;
        gInput.deltaY = 0;
    }
    else if (event.type == teWindowEvent::Type::Mouse2Up)
    {
        gInput.x = event.x;
        gInput.y = event.y;
        gInput.isRightMouseDown = false;
        gInput.deltaX = 0;
        gInput.deltaY = 0;
        gInput.lastMouseX = gInput.x;
        gInput.lastMouseY = gInput.y;
    }
    else if (event.type == teWindowEvent::Type::MouseMove)
    {
        gInput.x = event.x;
        gInput.y = event.y;
        gInput.deltaX = float( gInput.x - gInput.lastMouseX );
        gInput.deltaY = float( gInput.y - gInput.lastMouseY );
        gInput.lastMouseX = gInput.x;
        gInput.lastMouseY = gInput.y;

        if (gInput.isRightMouseDown)
        {
            teTransformOffsetRotate( gResources.camera3d.index, Vec3( 0, 1, 0 ), -gInput.deltaX / 100.0f * (float)gGameState.dt );
            teTransformOffsetRotate( gResources.camera3d.index, Vec3( 1, 0, 0 ), -gInput.deltaY / 100.0f * (float)gGameState.dt );
        }
    }
}
