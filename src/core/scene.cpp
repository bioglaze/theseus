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

void BeginRendering( teTexture2D& color, teTexture2D& depth, teClearFlag clearFlag, const float* clearColor );
void EndRendering( teTexture2D& color );
void PushGroupMarker( const char* name );
void PopGroupMarker();

void MeshRendererSetCulled( unsigned gameObjectIndex, unsigned subMeshIndex, bool isCulled );
bool MeshRendererIsCulled( unsigned gameObjectIndex, unsigned subMeshIndex );
void TransformSolveLocalMatrix( unsigned index, bool isCamera );
void teTransformGetComputedLocalToClipMatrix( unsigned index, Matrix& outLocalToClip );
void teTransformGetComputedLocalToViewMatrix( unsigned index, Matrix& outLocalToView );
void UpdateUBO( const float localToClip[ 16 ], const float localToView[ 16 ], const float localToShadowClip[ 16 ], const ShaderParams& shaderParams );
void Draw( const teShader& shader, unsigned positionOffset, unsigned uvOffset, unsigned indexCount, unsigned indexOffset, teBlendMode blendMode, teCullMode cullMode, teDepthMode depthMode, teTopology topology, teFillMode fillMode, teTextureFormat colorFormat, teTextureFormat depthFormat, unsigned textureIndex, teTextureSampler sampler, unsigned shadowMapIndex );
void TransformSetComputedLocalToClip( unsigned index, const Matrix& localToClip );
void TransformSetComputedLocalToView( unsigned index, const Matrix& localToView );
void GetCorners( const Vec3& min, const Vec3& max, Vec3 outCorners[ 8 ] );
void GetMinMax( const Vec3* aPoints, unsigned count, Vec3& outMin, Vec3& outMax );
teTextureFormat GetSwapchainColorFormat();

constexpr unsigned MAX_GAMEOBJECTS = 10000;

struct ShadowCaster
{
    teTexture2D color;
    teTexture2D depth;
    unsigned cameraIndex{ MAX_GAMEOBJECTS - 1 };
    Vec3 lightColor;
    Vec3 lightDirection;
};

struct SceneImpl
{
    unsigned gameObjects[ MAX_GAMEOBJECTS ] = {};
    ShadowCaster shadowCaster;
};

SceneImpl scenes[ 2 ];
unsigned sceneIndex = 0;
teMesh quadMesh;

teScene teCreateScene( unsigned directonalShadowMapDimension )
{
    teAssert( sceneIndex < 2 );

    teScene outScene;
    outScene.index = sceneIndex++;

    for (unsigned i = 0; i < MAX_GAMEOBJECTS; ++i)
    {
        scenes[ outScene.index ].gameObjects[ i ] = 0;
    }

    if (directonalShadowMapDimension != 0)
    {
        const unsigned cameraGOIndex = scenes[ outScene.index ].gameObjects[ scenes[ outScene.index ].shadowCaster.cameraIndex ];

        scenes[ outScene.index ].shadowCaster.color = teCreateTexture2D( directonalShadowMapDimension, directonalShadowMapDimension, teTextureFlags::RenderTexture, teTextureFormat::R32G32F, "dir shadow color" );
        scenes[ outScene.index ].shadowCaster.depth = teCreateTexture2D( directonalShadowMapDimension, directonalShadowMapDimension, teTextureFlags::RenderTexture, teTextureFormat::Depth32F, "dir shadow depth" );

        teCameraGetColorTexture( cameraGOIndex ) = scenes[ outScene.index ].shadowCaster.color;
        teCameraGetDepthTexture( cameraGOIndex ) = scenes[ outScene.index ].shadowCaster.depth;
    }

    quadMesh = teCreateQuadMesh();

    return outScene;
}

void teSceneSetupDirectionalLight( const teScene& scene, const Vec3& color, const Vec3& direction )
{
    scenes[ scene.index ].shadowCaster.lightColor = color;
    scenes[ scene.index ].shadowCaster.lightDirection = direction;
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

        TransformSetComputedLocalToClip( scenes[ scene.index ].gameObjects[ gameObjectIndex ], localToClip );
        TransformSetComputedLocalToView( scenes[ scene.index ].gameObjects[ gameObjectIndex ], localToView );

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
    const teTexture2D& color = teCameraGetColorTexture( cameraGOIndex );
    const teTexture2D& depth = teCameraGetDepthTexture( cameraGOIndex );

    Matrix localToView;
    Matrix localToShadowClip;

    Matrix localToClip;
    Matrix view;
    Quaternion cameraRot = teTransformGetLocalRotation( cameraGOIndex );
    cameraRot.GetMatrix( view );
    const Matrix& projection = teCameraGetProjection( cameraGOIndex );
    Matrix::Multiply( view, projection, localToClip );
    ShaderParams shaderParams{};
    UpdateUBO( localToClip.m, localToView.m, localToShadowClip.m, shaderParams );

    PushGroupMarker( "Skybox" );
    unsigned indexOffset = teMeshGetIndexOffset( *skyboxMesh, 0 );
    unsigned indexCount = teMeshGetIndexCount( *skyboxMesh, 0 );
    unsigned positionOffset = teMeshGetPositionOffset( *skyboxMesh, 0 );
    unsigned uvOffset = teMeshGetUVOffset( *skyboxMesh, 0 );
    
    Draw( *skyboxShader, positionOffset, uvOffset, indexCount, indexOffset, teBlendMode::Off, teCullMode::Off, teDepthMode::NoneWriteOff, teTopology::Triangles, teFillMode::Solid, color.format, depth.format, skyboxTexture->index, teTextureSampler::LinearRepeat, 0 );

    PopGroupMarker();
}

void teDrawQuad( const teShader& shader, teTexture2D texture, const ShaderParams& shaderParams, teBlendMode blendMode, unsigned cameraGOIndex )
{
    const teTexture2D& depth = teCameraGetDepthTexture( cameraGOIndex );

    Matrix identity;
    UpdateUBO( identity.m, identity.m, identity.m, shaderParams );

    PushGroupMarker( "Fullscreen Quad" );
    unsigned indexOffset = teMeshGetIndexOffset( quadMesh, 0 );
    unsigned indexCount = teMeshGetIndexCount( quadMesh, 0 );
    unsigned positionOffset = teMeshGetPositionOffset( quadMesh, 0 );
    unsigned uvOffset = teMeshGetUVOffset( quadMesh, 0 );

    Draw( shader, positionOffset, uvOffset, indexCount, indexOffset, blendMode, teCullMode::Off, teDepthMode::NoneWriteOff, teTopology::Triangles, teFillMode::Solid, GetSwapchainColorFormat(), depth.format, texture.index, teTextureSampler::LinearRepeat, 0);

    PopGroupMarker();
}

static void RenderMeshes( const teScene& scene, teBlendMode blendMode, teTextureFormat colorFormat, teTextureFormat depthFormat, unsigned shadowMapIndex )
{
    for (unsigned gameObjectIndex = 0; gameObjectIndex < MAX_GAMEOBJECTS; ++gameObjectIndex)
    {
        if (scenes[ scene.index ].gameObjects[ gameObjectIndex ] == 0 ||
            (teGameObjectGetComponents( scenes[ scene.index ].gameObjects[ gameObjectIndex ] ) & teComponent::MeshRenderer) == 0)
        {
            continue;
        }

        Matrix localToClip;
        teTransformGetComputedLocalToClipMatrix( scenes[ scene.index ].gameObjects[ gameObjectIndex ], localToClip );

        Matrix localToView;
        teTransformGetComputedLocalToViewMatrix( scenes[ scene.index ].gameObjects[ gameObjectIndex ], localToView );

        // TODO: get from transform/camera.
        Matrix localToShadowClip;

        const teMesh& mesh = teMeshRendererGetMesh( scenes[ scene.index ].gameObjects[ gameObjectIndex ] );

        for (unsigned subMeshIndex = 0; subMeshIndex < teMeshGetSubMeshCount( mesh ); ++subMeshIndex)
        {
            const teMaterial& material = teMeshRendererGetMaterial( scenes[ scene.index ].gameObjects[ gameObjectIndex ], subMeshIndex );

            if (MeshRendererIsCulled( scenes[ scene.index ].gameObjects[ gameObjectIndex ], subMeshIndex ) || material.blendMode != blendMode)
            {
                continue;
            }

            ShaderParams shaderParams{};
            UpdateUBO( localToClip.m, localToView.m, localToShadowClip.m, shaderParams );

            teShader shader = teMaterialGetShader( material );

            unsigned indexOffset = teMeshGetIndexOffset( mesh, subMeshIndex );
            unsigned indexCount = teMeshGetIndexCount( mesh, subMeshIndex );
            unsigned positionOffset = teMeshGetPositionOffset( mesh, subMeshIndex );
            unsigned uvOffset = teMeshGetUVOffset( mesh, subMeshIndex );

            teTexture2D texture = teMaterialGetTexture2D( material, 0 );

            Draw( shader, positionOffset, uvOffset, indexCount, indexOffset, material.blendMode, material.cullMode, material.depthMode, material.topology, material.fillMode, colorFormat, depthFormat, texture.index, texture.sampler, shadowMapIndex );
        }
    }
}

static void RenderSceneWithCamera( const teScene& scene, unsigned cameraIndex, const teShader* skyboxShader, const teTextureCube* skyboxTexture, const teMesh* skyboxMesh, unsigned shadowMapindex, const char* profileMarker )
{
    const unsigned cameraGOIndex = scenes[ scene.index ].gameObjects[ cameraIndex ];

    teTexture2D& color = teCameraGetColorTexture( cameraGOIndex );
    teTexture2D& depth = teCameraGetDepthTexture( cameraGOIndex );

    teAssert( color.index != 0 ); // Camera must have a render target!

    TransformSolveLocalMatrix( cameraGOIndex, true );
    UpdateFrustum( cameraGOIndex, -teTransformGetLocalPosition( cameraGOIndex ), teTransformGetViewDirection( cameraGOIndex ) );
    UpdateTransformsAndCull( scene, cameraGOIndex );

    teClearFlag clearFlag;
    Vec4 clearColor;
    teCameraGetClear( cameraGOIndex, clearFlag, clearColor );
    BeginRendering( color, depth, clearFlag, &clearColor.x );

    PushGroupMarker( profileMarker );

    if (skyboxShader && skyboxTexture && skyboxMesh)
    {
        RenderSky( cameraGOIndex, skyboxShader, skyboxTexture, skyboxMesh );
    }

    RenderMeshes( scene, teBlendMode::Off, color.format, depth.format, shadowMapindex );
    RenderMeshes( scene, teBlendMode::Alpha, color.format, depth.format, shadowMapindex );

    PopGroupMarker();

    EndRendering( color );
}

static void RenderDirLightShadow( const teScene& scene, Vec3& outColor, unsigned& outShadowMapIndex )
{
    const bool castShadowMap = scenes[ scene.index ].shadowCaster.color.index != 0;

    if (castShadowMap)
    {
        Vec3 tempDirLightPosition = Vec3( 1, 10, 1 );
        teTransformLookAt( scenes[ scene.index ].shadowCaster.cameraIndex, /*teTransformGetLocalPosition(dirLightIndex)*/tempDirLightPosition, tempDirLightPosition + scenes[ scene.index ].shadowCaster.lightDirection, {0, 1, 0});
        teCameraSetProjection( scenes[ scene.index ].shadowCaster.cameraIndex, 45, 1, 0.1f, 400.0f );

        RenderSceneWithCamera( scene, scenes[ scene.index ].shadowCaster.cameraIndex, nullptr, nullptr, nullptr, 0, "Shadow Map" );
    }

    outShadowMapIndex = castShadowMap ? scenes[ scene.index ].shadowCaster.color.index : 0;
}

void teSceneRender( const teScene& scene, const teShader* skyboxShader, const teTextureCube* skyboxTexture, const teMesh* skyboxMesh )
{
    Vec3 dirLightColor{ 1, 1, 1 };
    unsigned shadowMapIndex = 0;
    RenderDirLightShadow( scene, dirLightColor, shadowMapIndex );

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
        RenderSceneWithCamera( scene, cameraIndex, skyboxShader, skyboxTexture, skyboxMesh, shadowMapIndex, "Camera" );
    }
}

bool teScenePointInsideAABB( const teScene& scene, const Vec3& point )
{
    bool isInside = false;

    for (unsigned gameObjectIndex = 0; gameObjectIndex < MAX_GAMEOBJECTS; ++gameObjectIndex)
    {
        if (scenes[ scene.index ].gameObjects[ gameObjectIndex ] != 0 &&
            (teGameObjectGetComponents( scenes[ scene.index ].gameObjects[ gameObjectIndex ] ) & teComponent::Transform) != 0 &&
            (teGameObjectGetComponents( scenes[ scene.index ].gameObjects[ gameObjectIndex ] ) & teComponent::MeshRenderer) != 0)
        {
            const Matrix localToWorld = teTransformGetMatrix( scenes[ scene.index ].gameObjects[ gameObjectIndex ] );

            const teMesh& mesh = teMeshRendererGetMesh( scenes[ scene.index ].gameObjects[ gameObjectIndex ] );

            for (unsigned subMeshIndex = 0; subMeshIndex < teMeshGetSubMeshCount( mesh ); ++subMeshIndex)
            {
                Vec3 meshAabbMinWorld, meshAabbMaxWorld;
                teMeshGetSubMeshLocalAABB( mesh, subMeshIndex, meshAabbMinWorld, meshAabbMaxWorld );

                // TODO: This could use the results of updateFrustumsAndCull() instead of calculating this again.
                Vec3 meshAabbWorld[ 8 ];
                GetCorners( meshAabbMinWorld, meshAabbMaxWorld, meshAabbWorld );

                for (unsigned v = 0; v < 8; ++v)
                {
                    Vec3 res;
                    Matrix::TransformPoint( meshAabbWorld[ v ], localToWorld, res );
                    meshAabbWorld[ v ] = res;
                }

                GetMinMax( meshAabbWorld, 8, meshAabbMinWorld, meshAabbMaxWorld );

                if (point.x > meshAabbMinWorld.x && point.x < meshAabbMaxWorld.x &&
                    point.y > meshAabbMinWorld.y && point.y < meshAabbMaxWorld.y &&
                    point.z > meshAabbMinWorld.z && point.z < meshAabbMaxWorld.z )
                {
                    return true;
                }
            }
        }
    }

    return isInside;
}
