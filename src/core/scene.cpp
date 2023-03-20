#include "scene.h"
#include "camera.h"
#include "frustum.h"
#include "gameobject.h"
#include "material.h"
#include "matrix.h"
#include "mesh.h"
#include "quaternion.h"
#include "renderer.h"
#include "shader.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"

void BeginRendering( teTexture2D& color, teTexture2D& depth );
void EndRendering( teTexture2D& color );

void TransformSolveLocalMatrix( unsigned index, bool isCamera );
void teTransformGetComputedLocalToClipMatrix( unsigned index, Matrix& outLocalToClipLeftEye, Matrix& outLocalToClipRightEye );
void teTransformGetComputedLocalToViewMatrix( unsigned index, Matrix& outLocalToViewLeftEye, Matrix& outLocalToViewRightEye );
void UpdateUBO( const float localToClip0[ 16 ], const float localToClip1[ 16 ], const float localToView0[ 16 ], const float localToView1[ 16 ] );
void Draw( const teShader& shader, unsigned positionOffset, unsigned indexCount, unsigned indexOffset, teBlendMode blendMode, teCullMode cullMode, teDepthMode depthMode, teTopology topology, teFillMode fillMode, teTextureFormat colorFormat, teTextureFormat depthFormat );

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
    unsigned cameraGOIndex = 0;

    cameraGOIndex = scenes[ scene.index ].gameObjects[ cameraIndex ];
    teTexture2D& color = teCameraGetColorTexture( cameraGOIndex );
    teTexture2D& depth = teCameraGetDepthTexture( cameraGOIndex );

    teAssert( color.index != -1 ); // Camera must have a render target!

    TransformSolveLocalMatrix( cameraGOIndex, true );
    UpdateFrustum( cameraGOIndex, -teTransformGetLocalPosition( cameraGOIndex ), teTransformGetViewDirection( cameraGOIndex ) );
    //UpdateTransformsAndCull( scene, cameraGOIndex );

    BeginRendering( color, depth );

    // this will be replaced by indirect rendering.
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

            const teMesh& mesh = teMeshRendererGetMesh( scenes[ scene.index ].gameObjects[ gameObjectIndex ] );

            for (unsigned subMeshIndex = 0; subMeshIndex < teMeshGetSubMeshCount( mesh ); ++subMeshIndex)
            {
                const teMaterial& material = teMeshRendererGetMaterial( scenes[ scene.index ].gameObjects[ gameObjectIndex ], subMeshIndex );
                teShader shader = teMaterialGetShader( material );

                unsigned indexOffset = teMeshGetIndexOffset( mesh.index, subMeshIndex );
                unsigned indexCount = teMeshGetIndexCount( mesh.index, subMeshIndex );
                unsigned positionOffset = teMeshGetPositionOffset( mesh.index, subMeshIndex );

                Draw( shader, positionOffset, indexCount, indexOffset, material.blendMode, material.cullMode, material.depthMode, material.topology, material.fillMode, color.format, depth.format );
            }
        }
    }

    EndRendering( color );
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
