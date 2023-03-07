#include "scene.h"
#include "camera.h"
#include "frustum.h"
#include "gameobject.h"
#include "matrix.h"
#include "mesh.h"
#include "quaternion.h"
#include "renderer.h"
#include "shader.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"

void BeginRendering( teTexture2D color, teTexture2D depth );
void EndRendering();

void TransformSolveLocalMatrix( unsigned index, bool isCamera );
void teTransformGetComputedLocalToClipMatrix( unsigned index, Matrix& outLocalToClipLeftEye, Matrix& outLocalToClipRightEye );
void teTransformGetComputedLocalToViewMatrix( unsigned index, Matrix& outLocalToViewLeftEye, Matrix& outLocalToViewRightEye );
void UpdateUBO( const float localToClip0[ 16 ], const float localToClip1[ 16 ], const float localToView0[ 16 ], const float localToView1[ 16 ] );

constexpr unsigned MAX_GAMEOBJECTS = 10000;

struct SceneImpl
{
    unsigned gameObjects[ MAX_GAMEOBJECTS ] = {};
};

SceneImpl scenes[ 2 ];
unsigned sceneIndex = 0;

teScene teCreateScene()
{
    teAssert( sceneIndex < 2 );

    teScene outScene;
    outScene.index = sceneIndex++;

    for (unsigned i = 0; i < MAX_GAMEOBJECTS; ++i)
    {
        scenes[ outScene.index ].gameObjects[ i ] = 0;
    }

    return outScene;
}

void teSceneAdd( const teScene& scene, unsigned gameObjectIndex )
{
    teAssert( scene.index < 2 );

    if ((teGameObjectGetComponents( gameObjectIndex ) & teComponent::Camera) != 0)
    {
        teAssert( teCameraGetColorTexture( gameObjectIndex ).index != -1 ); // Camera must have a render texture!
    }

    for (unsigned i = 0; i < MAX_GAMEOBJECTS; ++i)
    {
        if (scenes[ scene.index ].gameObjects[ i ] == gameObjectIndex)
        {
            return;
        }
    }

    for (unsigned i = 0; i < MAX_GAMEOBJECTS; ++i)
    {
        if (scenes[ scene.index ].gameObjects[ i ] == 0)
        {
            scenes[ scene.index ].gameObjects[ i ] = gameObjectIndex;
            return;
        }
    }

    teAssert( !"Too many game objects!" );
}

static void RenderSceneWithCamera( const teScene& scene, unsigned cameraIndex )
{
    teTexture2D color;
    teTexture2D depth;

    unsigned cameraGOIndex = 0;

    {
        cameraGOIndex = scenes[ scene.index ].gameObjects[ cameraIndex ];
        color = teCameraGetColorTexture( cameraGOIndex );
        depth = teCameraGetDepthTexture( cameraGOIndex );
    }

    teAssert( color.index != -1 ); // Camera must have a render target!

    TransformSolveLocalMatrix( cameraGOIndex, true );
    UpdateFrustum( cameraGOIndex, -teTransformGetLocalPosition( cameraGOIndex ), teTransformGetViewDirection( cameraGOIndex ) );
    //UpdateTransformsAndCull( scene, cameraGOIndex );

    BeginRendering( color, depth );

    // fdsfdsf
    {
        for (unsigned gameObjectIndex = 0; gameObjectIndex < MAX_GAMEOBJECTS; ++gameObjectIndex)
        {
            if (scenes[ scene.index ].gameObjects[ gameObjectIndex ] == 0 ||
                (teGameObjectGetComponents( scenes[ scene.index ].gameObjects[ gameObjectIndex ] ) & teComponent::MeshRenderer) == 0)
            {
                continue;
            }

            Matrix localToClipLeft, localToClipRight;
            teTransformGetComputedLocalToClipMatrix( scenes[ scene.index ].gameObjects[ gameObjectIndex ], localToClipLeft, localToClipRight );

            Matrix localToViewLeft, localToViewRight;
            teTransformGetComputedLocalToViewMatrix( scenes[ scene.index ].gameObjects[ gameObjectIndex ], localToViewLeft, localToViewRight );

            UpdateUBO( localToClipLeft.m, localToClipRight.m, localToViewLeft.m, localToViewRight.m );

            //const teMesh& mesh = teMeshRendererGetMesh( scenes[ scene.index ].gameObjects[ gameObjectIndex ] );

            //for (unsigned subMeshIndex = 0; subMeshIndex < teMeshGetSubMeshCount( mesh ); ++subMeshIndex)
            {

            }
        }
    }

    EndRendering();
}

void teSceneRender( const teScene& scene )
{
    int cameraIndex = -1;

    for (unsigned i = 0; i < MAX_GAMEOBJECTS; ++i)
    {
        if (scenes[ scene.index ].gameObjects[ i ] != 0 &&
            (teGameObjectGetComponents( scenes[ scene.index ].gameObjects[ i ] ) & teComponent::Camera) != 0)
        {
            cameraIndex = i;
            break;
        }
    }

    if (cameraIndex != -1)
    {
        RenderSceneWithCamera( scene, cameraIndex );
    }
}
