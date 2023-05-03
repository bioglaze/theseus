#include "camera.h"
#include "file.h"
#include "gameobject.h"
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

struct AppResources
{
    teShader unlitShader;
    teShader fullscreenShader;
    teShader skyboxShader;
    
    teGameObject camera3d;
    teGameObject cubeGo;
    teGameObject cubes[ 16 * 4 ];
    teMaterial material;
    teMesh cubeMesh;
    teTexture2D gliderTex;
    teTexture2D bc1Tex;
    teTexture2D bc2Tex;
    teTexture2D bc3Tex;
    teTextureCube skyTex;
    teScene scene;
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
    app.unlitShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "unlitVS", "unlitPS" );
    app.skyboxShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "skyboxVS", "skyboxPS" );
    
    app.camera3d = teCreateGameObject( "camera3d", teComponent::Transform | teComponent::Camera );
    Vec3 cameraPos = { 0, 0, -10 };
    teTransformSetLocalPosition( app.camera3d.index, cameraPos );
    teCameraSetProjection( app.camera3d.index, 45, width / (float)height, 0.1f, 800.0f );
    teCameraSetClear( app.camera3d.index, teClearFlag::DepthAndColor, Vec4( 1, 0, 0, 1 ) );
    teCameraGetColorTexture( app.camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::BGRA_sRGB, "camera3d color" );
    teCameraGetDepthTexture( app.camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::Depth32F, "camera3d depth" );
    
    app.gliderTex = teLoadTexture( teLoadFile( "assets/textures/glider_color.tga" ), teTextureFlags::GenerateMips );
    app.bc1Tex = teLoadTexture( teLoadFile( "assets/textures/test/test_dxt1.dds" ), teTextureFlags::GenerateMips );
    app.bc2Tex = teLoadTexture( teLoadFile( "assets/textures/test/test_dxt3.dds" ), teTextureFlags::GenerateMips );
    app.bc3Tex = teLoadTexture( teLoadFile( "assets/textures/test/test_dxt5.dds" ), teTextureFlags::GenerateMips );
    
    app.material = teCreateMaterial( app.unlitShader );
    teMaterialSetTexture2D( app.material, app.bc1Tex, 0 );
    
    app.cubeMesh = teCreateCubeMesh();
    app.cubeGo = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
    teMeshRendererSetMesh( app.cubeGo.index, &app.cubeMesh );
    teMeshRendererSetMaterial( app.cubeGo.index, app.material, 0 );

    teFile backFile = teLoadFile( "assets/textures/skybox/back.dds" );
    teFile frontFile = teLoadFile( "assets/textures/skybox/front.dds" );
    teFile leftFile = teLoadFile( "assets/textures/skybox/left.dds" );
    teFile rightFile = teLoadFile( "assets/textures/skybox/right.dds" );
    teFile topFile = teLoadFile( "assets/textures/skybox/top.dds" );
    teFile bottomFile = teLoadFile( "assets/textures/skybox/bottom.dds" );
    
    app.skyTex = teLoadTexture( leftFile, rightFile, bottomFile, topFile, frontFile, backFile, 0 );

    app.scene = teCreateScene();
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

}

void DrawApp()
{
    float dt = 1;
    
    teTransformMoveForward( app.camera3d.index, moveDir.z * (float)dt * 0.5f );
    teTransformMoveRight( app.camera3d.index, moveDir.x * (float)dt * 0.5f );
    teTransformMoveUp( app.camera3d.index, moveDir.y * (float)dt * 0.5f );

    teBeginFrame();
    //teSceneRender( scene, NULL, NULL, NULL );
    teSceneRender( app.scene, &app.skyboxShader, &app.skyTex, &app.cubeMesh );
    teBeginSwapchainRendering( teCameraGetColorTexture( app.camera3d.index ) );
    teDrawFullscreenTriangle( app.fullscreenShader, teCameraGetColorTexture( app.camera3d.index ) );
    teEndSwapchainRendering();
}
