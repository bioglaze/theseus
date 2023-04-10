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
#include "te_stdlib.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"

void BeginRendering( teTexture2D& color, teTexture2D& depth );
void EndRendering( teTexture2D& color );
void PushGroupMarker( const char* name );
void PopGroupMarker();

void MeshRendererSetCulled( unsigned gameObjectIndex, unsigned subMeshIndex, bool isCulled );
bool MeshRendererIsCulled( unsigned gameObjectIndex, unsigned subMeshIndex );
void TransformSolveLocalMatrix( unsigned index, bool isCamera );
void teTransformGetComputedLocalToClipMatrix( unsigned index, Matrix& outLocalToClipLeftEye, Matrix& outLocalToClipRightEye );
void teTransformGetComputedLocalToViewMatrix( unsigned index, Matrix& outLocalToViewLeftEye, Matrix& outLocalToViewRightEye );
void UpdateUBO( const float localToClip0[ 16 ], const float localToClip1[ 16 ], const float localToView0[ 16 ], const float localToView1[ 16 ] );
void Draw( const teShader& shader, unsigned positionOffset, unsigned indexCount, unsigned indexOffset, teBlendMode blendMode, teCullMode cullMode, teDepthMode depthMode, teTopology topology, teFillMode fillMode, teTextureFormat colorFormat, teTextureFormat depthFormat, unsigned textureIndex );
void TransformSetComputedLocalToClip( unsigned index, const Matrix& localToClipLeftEye, const Matrix& localToClipRightEye );
void TransformSetComputedLocalToView( unsigned index, const Matrix& localToViewLeftEye, const Matrix& localToViewRightEye );
void GetCorners( const Vec3& min, const Vec3& max, Vec3 outCorners[ 8 ] );
void GetMinMax( const Vec3* aPoints, unsigned count, Vec3& outMin, Vec3& outMax );

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
        teAssert( teCameraGetColorTexture( gameObjectIndex ).index != 0 ); // Camera must have a render texture!
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

static void UpdateTransformsAndCull( const teScene& scene, unsigned cameraGOIndex )
{
    for (unsigned gameObjectIndex = 0; gameObjectIndex < MAX_GAMEOBJECTS; ++gameObjectIndex)
    {
        if (scenes[ scene.index ].gameObjects[ gameObjectIndex ] == 0 ||
            (teGameObjectGetComponents( scenes[ scene.index ].gameObjects[ gameObjectIndex ] ) & teComponent::MeshRenderer) == 0)
        {
            continue;
        }

        TransformSolveLocalMatrix( scenes[ scene.index ].gameObjects[ gameObjectIndex ], false );

        const Matrix localToWorld = teTransformGetMatrix( scenes[ scene.index ].gameObjects[ gameObjectIndex ] );

        Matrix localToView;
        Matrix localToClip;
        Matrix::Multiply( localToWorld, teTransformGetMatrix( cameraGOIndex ), localToView );
        Matrix::Multiply( localToView, teCameraGetProjection( cameraGOIndex ), localToClip );

        TransformSetComputedLocalToClip( scenes[ scene.index ].gameObjects[ gameObjectIndex ], localToClip, localToClip );
        TransformSetComputedLocalToView( scenes[ scene.index ].gameObjects[ gameObjectIndex ], localToView, localToView );

        const teMesh& mesh = teMeshRendererGetMesh(scenes[scene.index].gameObjects[ gameObjectIndex ] );

        for (unsigned subMeshIndex = 0; subMeshIndex < teMeshGetSubMeshCount( mesh ); ++subMeshIndex)
        {
            Vec3 meshAabbMinWorld, meshAabbMaxWorld;
            teMeshGetSubMeshLocalAABB( mesh, subMeshIndex, meshAabbMinWorld, meshAabbMaxWorld );

            Vec3 meshAabbWorld[ 8 ];
            GetCorners( meshAabbMinWorld, meshAabbMaxWorld, meshAabbWorld );

            for (unsigned v = 0; v < 8; ++v)
            {
                Vec3 res;
                Matrix::TransformPoint( meshAabbWorld[ v ], localToWorld, res );
                meshAabbWorld[ v ] = res;
            }

            GetMinMax( meshAabbWorld, 8, meshAabbMinWorld, meshAabbMaxWorld );

            MeshRendererSetCulled( scenes[ scene.index ].gameObjects[ gameObjectIndex ], subMeshIndex, !BoxInFrustum( cameraGOIndex, meshAabbMinWorld, meshAabbMaxWorld ) );
        }
    }
}

static void RenderSky( unsigned cameraGOIndex, const teShader* skyboxShader, const teTextureCube* skyboxTexture, const teMesh* skyboxMesh )
{
    teTexture2D& color = teCameraGetColorTexture( cameraGOIndex );
    teTexture2D& depth = teCameraGetDepthTexture( cameraGOIndex );

    Matrix localToViews[ 2 ];

    Matrix localToClip;
    Matrix view;
    Quaternion cameraRot = teTransformGetLocalRotation( cameraGOIndex );
    cameraRot.GetMatrix( view );
    const Matrix& projection = teCameraGetProjection( cameraGOIndex );
    Matrix::Multiply( view, projection, localToClip );
    UpdateUBO( localToClip.m, localToClip.m, localToViews[ 0 ].m, localToViews[ 1 ].m );

    PushGroupMarker( "Skybox" );
    unsigned indexOffset = teMeshGetIndexOffset( *skyboxMesh, 0 );
    unsigned indexCount = teMeshGetIndexCount( *skyboxMesh, 0 );
    unsigned positionOffset = teMeshGetPositionOffset( *skyboxMesh, 0 );

    Draw( *skyboxShader, positionOffset, indexCount, indexOffset, teBlendMode::Off, teCullMode::Off, teDepthMode::NoneWriteOff, teTopology::Triangles, teFillMode::Solid, color.format, depth.format, skyboxTexture->index );

    PopGroupMarker();
}

static void RenderSceneWithCamera( const teScene& scene, unsigned cameraIndex, const teShader* skyboxShader, const teTextureCube* skyboxTexture, const teMesh* skyboxMesh )
{
    const unsigned cameraGOIndex = scenes[ scene.index ].gameObjects[ cameraIndex ];

    teTexture2D& color = teCameraGetColorTexture( cameraGOIndex );
    teTexture2D& depth = teCameraGetDepthTexture( cameraGOIndex );

    teAssert( color.index != 0 ); // Camera must have a render target!

    TransformSolveLocalMatrix( cameraGOIndex, true );
    UpdateFrustum( cameraGOIndex, -teTransformGetLocalPosition( cameraGOIndex ), teTransformGetViewDirection( cameraGOIndex ) );
    UpdateTransformsAndCull( scene, cameraGOIndex );

    BeginRendering( color, depth );

    if (skyboxShader && skyboxTexture && skyboxMesh)
    {
        RenderSky( cameraGOIndex, skyboxShader, skyboxTexture, skyboxMesh );
    }

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
                
                if (MeshRendererIsCulled( scenes[ scene.index ].gameObjects[ gameObjectIndex ], subMeshIndex ))
                {
                    continue;
                }

                teShader shader = teMaterialGetShader( material );

                unsigned indexOffset = teMeshGetIndexOffset( mesh, subMeshIndex );
                unsigned indexCount = teMeshGetIndexCount( mesh, subMeshIndex );
                unsigned positionOffset = teMeshGetPositionOffset( mesh, subMeshIndex );

                unsigned textureIndex = teMaterialGetTexture2D( material, 0 );

                Draw( shader, positionOffset, indexCount, indexOffset, material.blendMode, material.cullMode, material.depthMode, material.topology, material.fillMode, color.format, depth.format, textureIndex );
            }
        }
    }

    EndRendering( color );
}

void teSceneRender( const teScene& scene, const teShader* skyboxShader, const teTextureCube* skyboxTexture, const teMesh* skyboxMesh )
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
        RenderSceneWithCamera( scene, cameraIndex, skyboxShader, skyboxTexture, skyboxMesh );
    }
}
