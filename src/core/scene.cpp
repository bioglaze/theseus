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
void teTransformSetComputedLocalToShadowClipMatrix( unsigned index, const Matrix& localToShadowClip );
void teTransformGetComputedLocalToShadowClipMatrix( unsigned index, Matrix& outLocalToShadowClip );
void UpdateUBO( const float localToClip[ 16 ], const float localToView[ 16 ], const float localToShadowClip[ 16 ], const float localToWorld[ 16 ], const ShaderParams& shaderParams, const Vec4& lightDirection, const Vec4& lightColor, const Vec4& lightPosition );
void Draw( const teShader& shader, unsigned positionOffset, unsigned uvOffset, unsigned normalOffset, unsigned tangentOffset, unsigned indexCount, unsigned indexOffset, teBlendMode blendMode, teCullMode cullMode, teDepthMode depthMode, teTopology topology, teFillMode fillMode, unsigned textureIndex, teTextureSampler sampler, unsigned normalMapIndex, unsigned shadowMapIndex );
void TransformSetComputedLocalToClip( unsigned index, const Matrix& localToClip );
void TransformSetComputedLocalToView( unsigned index, const Matrix& localToView );
void teGetCorners( const Vec3& min, const Vec3& max, Vec3 outCorners[ 8 ] );
void GetMinMax( const Vec3* aPoints, unsigned count, Vec3& outMin, Vec3& outMax );
unsigned teMeshGetPositionOffset( const teMesh& mesh, unsigned subMeshIndex );
unsigned teMeshGetNormalOffset( const teMesh& mesh, unsigned subMeshIndex );
unsigned teMeshGetIndexOffset( const teMesh& mesh, unsigned subMeshIndex );
unsigned teMeshGetUVOffset( const teMesh& mesh, unsigned subMeshIndex );
unsigned teMeshGetTangentOffset( const teMesh& mesh, unsigned subMeshIndex );

constexpr unsigned MAX_GAMEOBJECTS = 10000;

struct ShadowCaster
{
    teTexture2D color;
    teTexture2D depth;
    unsigned cameraIndex{ MAX_GAMEOBJECTS - 1 };
    Vec3 lightDirection;
};

struct SceneImpl
{
    unsigned gameObjects[ MAX_GAMEOBJECTS ] = {};
    ShadowCaster shadowCaster;
    Vec3 directionalLightColor;
    Vec3 directionalLightDirection;
    Vec3 directionalLightPosition;
};

SceneImpl scenes[ 2 ];
unsigned sceneIndex = 0;
teMesh quadMesh;

unsigned teSceneGetMaxGameObjects()
{
    return MAX_GAMEOBJECTS;
}

unsigned teSceneGetGameObjectIndex( const teScene& scene, unsigned i )
{
    return scenes[ scene.index ].gameObjects[ i ];
}

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
        scenes[ outScene.index ].shadowCaster.depth = teCreateTexture2D( directonalShadowMapDimension, directonalShadowMapDimension, teTextureFlags::RenderTexture, teTextureFormat::Depth32F_S8, "dir shadow depth" );

        teCameraGetColorTexture( cameraGOIndex ) = scenes[ outScene.index ].shadowCaster.color;
        teCameraGetDepthTexture( cameraGOIndex ) = scenes[ outScene.index ].shadowCaster.depth;
    }

    quadMesh = teCreateQuadMesh();

    return outScene;
}

void teSceneSetupDirectionalLight( const teScene& scene, const Vec3& color, const Vec3& direction )
{
    scenes[ scene.index ].shadowCaster.lightDirection = direction;
    scenes[ scene.index ].directionalLightDirection = direction;
    scenes[ scene.index ].directionalLightColor = color;
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

void teSceneRemove( const teScene& scene, unsigned gameObjectIndex )
{
    teAssert( scene.index < 2 );

    for (unsigned i = 0; i < MAX_GAMEOBJECTS; ++i)
    {
        if (scenes[ scene.index ].gameObjects[ i ] == gameObjectIndex)
        {
            scenes[ scene.index ].gameObjects[ i ] = 0;
        }
    }
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

        Matrix localToShadowClip;

        if (cameraGOIndex != scenes[ scene.index ].shadowCaster.cameraIndex)
        {
            unsigned goIndex = scenes[ scene.index ].gameObjects[ scenes[ scene.index ].shadowCaster.cameraIndex ];

            Matrix::Multiply( localToWorld, teTransformGetMatrix( goIndex ), localToShadowClip );
            Matrix::Multiply( localToShadowClip, teCameraGetProjection( goIndex ), localToShadowClip );
        }

        teTransformSetComputedLocalToShadowClipMatrix( scenes[ scene.index ].gameObjects[ gameObjectIndex ], localToShadowClip );

        const teMesh* mesh = teMeshRendererGetMesh( scenes[ scene.index ].gameObjects[ gameObjectIndex ] );

        for (unsigned subMeshIndex = 0; subMeshIndex < teMeshGetSubMeshCount( mesh ); ++subMeshIndex)
        {
            Vec3 meshAabbMinWorld, meshAabbMaxWorld;
            teMeshGetSubMeshLocalAABB( *mesh, subMeshIndex, meshAabbMinWorld, meshAabbMaxWorld );

            Vec3 meshAabbWorld[ 8 ];
            teGetCorners( meshAabbMinWorld, meshAabbMaxWorld, meshAabbWorld );

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
    Matrix localToView;
    Matrix localToShadowClip;

    Matrix localToClip;
    Matrix view;
    Quaternion cameraRot = teTransformGetLocalRotation( cameraGOIndex );
    cameraRot.GetMatrix( view );
    const Matrix& projection = teCameraGetProjection( cameraGOIndex );
    Matrix::Multiply( view, projection, localToClip );
    ShaderParams shaderParams{};
    UpdateUBO( localToClip.m, localToView.m, localToShadowClip.m, localToView.m, shaderParams, Vec4( 0, 0, 0, 1 ), Vec4( 1, 1, 1, 1 ), Vec4( 1, 1, 1, 1 ) );

    PushGroupMarker( "Skybox" );
    unsigned indexOffset = teMeshGetIndexOffset( *skyboxMesh, 0 );
    unsigned indexCount = teMeshGetIndexCount( *skyboxMesh, 0 );
    unsigned positionOffset = teMeshGetPositionOffset( *skyboxMesh, 0 );
    unsigned normalOffset = teMeshGetNormalOffset( *skyboxMesh, 0 );
    unsigned uvOffset = teMeshGetUVOffset( *skyboxMesh, 0 );
    
    Draw( *skyboxShader, positionOffset, uvOffset, normalOffset, 0, indexCount, indexOffset, teBlendMode::Off, teCullMode::Off, teDepthMode::NoneWriteOff, teTopology::Triangles, teFillMode::Solid, skyboxTexture->index, teTextureSampler::LinearRepeat, 0, skyboxTexture->index );

    PopGroupMarker();
}

void teDrawQuad( const teShader& shader, teTexture2D texture, const ShaderParams& shaderParams, teBlendMode blendMode )
{
    Matrix identity;
    UpdateUBO( identity.m, identity.m, identity.m, identity.m, shaderParams, Vec4( 0, 0, 0, 1 ), Vec4( 1, 1, 1, 1 ), Vec4( 1, 1, 1, 1 ) );

    PushGroupMarker( "Fullscreen Quad" );
    unsigned indexOffset = teMeshGetIndexOffset( quadMesh, 0 );
    unsigned indexCount = teMeshGetIndexCount( quadMesh, 0 );
    unsigned positionOffset = teMeshGetPositionOffset( quadMesh, 0 );
    unsigned normalOffset = teMeshGetNormalOffset( quadMesh, 0 );
    unsigned uvOffset = teMeshGetUVOffset( quadMesh, 0 );

    Draw( shader, positionOffset, uvOffset, normalOffset, 0, indexCount, indexOffset, blendMode, teCullMode::Off, teDepthMode::NoneWriteOff, teTopology::Triangles, teFillMode::Solid, texture.index, teTextureSampler::LinearRepeat, 0, texture.index );

    PopGroupMarker();
}

static void RenderMeshes( const teScene& scene, teBlendMode blendMode, unsigned shadowMapIndex, const teShader* momentsShader )
{
    for (unsigned gameObjectIndex = 0; gameObjectIndex < MAX_GAMEOBJECTS; ++gameObjectIndex)
    {
        if (scenes[ scene.index ].gameObjects[ gameObjectIndex ] == 0 ||
            (teGameObjectGetComponents( scenes[ scene.index ].gameObjects[ gameObjectIndex ] ) & teComponent::MeshRenderer) == 0)
        {
            continue;
        }

        if (!teMeshRendererIsEnabled( scenes[ scene.index ].gameObjects[ gameObjectIndex ] ))
        {
            continue;
        }
        
        Matrix localToClip;
        teTransformGetComputedLocalToClipMatrix( scenes[ scene.index ].gameObjects[ gameObjectIndex ], localToClip );

        Matrix localToView;
        teTransformGetComputedLocalToViewMatrix( scenes[ scene.index ].gameObjects[ gameObjectIndex ], localToView );

        Matrix localToShadowClip;
        teTransformGetComputedLocalToShadowClipMatrix( scenes[ scene.index ].gameObjects[ gameObjectIndex ], localToShadowClip );

        Matrix localToWorld = teTransformGetMatrix( scenes[ scene.index ].gameObjects[ gameObjectIndex ] );

        const teMesh* mesh = teMeshRendererGetMesh( scenes[ scene.index ].gameObjects[ gameObjectIndex ] );

        for (unsigned subMeshIndex = 0; subMeshIndex < teMeshGetSubMeshCount( mesh ); ++subMeshIndex)
        {
            const teMaterial& material = teMeshRendererGetMaterial( scenes[ scene.index ].gameObjects[ gameObjectIndex ], subMeshIndex );

            if (MeshRendererIsCulled( scenes[ scene.index ].gameObjects[ gameObjectIndex ], subMeshIndex ) || material.blendMode != blendMode)
            {
                continue;
            }

            ShaderParams shaderParams{};
            Vec4 tint = teMaterialGetTint( material );
            shaderParams.tint[ 0 ] = tint.x;
            shaderParams.tint[ 1 ] = tint.y;
            shaderParams.tint[ 2 ] = tint.z;
            shaderParams.tint[ 3 ] = tint.w;
            Vec4 lightDir;
            lightDir.x = scenes[ scene.index ].directionalLightDirection.x;
            lightDir.y = scenes[ scene.index ].directionalLightDirection.y;
            lightDir.z = scenes[ scene.index ].directionalLightDirection.z;
            Vec4 lightColor;
            lightColor.x = scenes[ scene.index ].directionalLightColor.x;
            lightColor.y = scenes[ scene.index ].directionalLightColor.y;
            lightColor.z = scenes[ scene.index ].directionalLightColor.z;
            Vec4 lightPosition;
            lightPosition.x = scenes[ scene.index ].directionalLightPosition.x;
            lightPosition.y = scenes[ scene.index ].directionalLightPosition.y;
            lightPosition.z = scenes[ scene.index ].directionalLightPosition.z;

            UpdateUBO( localToClip.m, localToView.m, localToShadowClip.m, localToWorld.m, shaderParams, lightDir, lightColor, lightPosition );

            const teShader shader = momentsShader ? *momentsShader : teMaterialGetShader( material );

            unsigned indexOffset = teMeshGetIndexOffset( *mesh, subMeshIndex );
            unsigned indexCount = teMeshGetIndexCount( *mesh, subMeshIndex );
            unsigned positionOffset = teMeshGetPositionOffset( *mesh, subMeshIndex );
            unsigned normalOffset = teMeshGetNormalOffset( *mesh, subMeshIndex );
            unsigned uvOffset = teMeshGetUVOffset( *mesh, subMeshIndex );
            unsigned tangentOffset = teMeshGetTangentOffset( *mesh, subMeshIndex );

            teTexture2D texture = teMaterialGetTexture2D( material, 0 );
            teTexture2D normalMap = teMaterialGetTexture2D( material, 1 );

            Draw( shader, positionOffset, uvOffset, normalOffset, tangentOffset, indexCount, indexOffset, material.blendMode, material.cullMode, material.depthMode, mesh->topology, material.fillMode, texture.index, texture.sampler, normalMap.index, shadowMapIndex );
        }
    }
}

// \param skyboxShader if not null, skybox is rendered using it, skyboxMesh and skyboxTexture.
// \param momentsShader if not null, overrides material's shader
static void RenderSceneWithCamera( const teScene& scene, unsigned cameraGOIndex, const teShader* skyboxShader, const teTextureCube* skyboxTexture, const teMesh* skyboxMesh, unsigned shadowMapindex, const char* profileMarker, const teShader* momentsShader )
{
    teTexture2D& color = teCameraGetColorTexture( cameraGOIndex );
    teTexture2D& depth = teCameraGetDepthTexture( cameraGOIndex );

    teAssert( color.index != 0 ); // Camera must have a render target!

    TransformSolveLocalMatrix( cameraGOIndex, true );
    UpdateFrustum( cameraGOIndex, teTransformGetLocalPosition( cameraGOIndex ), teTransformGetViewDirection( cameraGOIndex ) );
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

    RenderMeshes( scene, teBlendMode::Off, shadowMapindex, momentsShader );
    RenderMeshes( scene, teBlendMode::Alpha, shadowMapindex, momentsShader );

    PopGroupMarker();

    EndRendering( color );
}

static void RenderDirLightShadow( const teScene& scene, const teShader& momentsShader, const Vec3& dirLightPosition, Vec3& outColor, unsigned& outShadowMapIndex )
{
    const bool castShadowMap = scenes[ scene.index ].shadowCaster.color.index != 0;

    if (castShadowMap)
    {
        scenes[ scene.index ].directionalLightPosition = dirLightPosition;

        unsigned index = scenes[ scene.index ].gameObjects[ scenes[ scene.index ].shadowCaster.cameraIndex ];
        teTransformLookAt( index, dirLightPosition, dirLightPosition - scenes[ scene.index ].shadowCaster.lightDirection, {0, 1, 0});
        teCameraSetProjection( index, 45, 1, 0.1f, 400.0f );

        TransformSolveLocalMatrix( index, true );
        const Matrix localToWorld = teTransformGetMatrix( index );

        Matrix localToView;
        Matrix localToClip;
        Matrix::Multiply( localToWorld, teTransformGetMatrix( index ), localToView );
        Matrix::Multiply( localToView, teCameraGetProjection( index ), localToClip );

        TransformSetComputedLocalToClip( index, localToClip );
        TransformSetComputedLocalToView( index, localToView );

        RenderSceneWithCamera( scene, index, nullptr, nullptr, nullptr, 0, "Shadow Map", &momentsShader );
    }

    outShadowMapIndex = castShadowMap ? scenes[ scene.index ].shadowCaster.color.index : 0;
}

void teSceneRender( const teScene& scene, const teShader* skyboxShader, const teTextureCube* skyboxTexture, const teMesh* skyboxMesh, const teShader& momentsShader, const Vec3& dirLightPosition )
{
    Vec3 dirLightColor{ 1, 1, 1 };
    unsigned shadowMapIndex = 0;
    RenderDirLightShadow( scene, momentsShader, dirLightPosition, dirLightColor, shadowMapIndex );

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
        RenderSceneWithCamera( scene, scenes[ scene.index ].gameObjects[ cameraIndex ], skyboxShader, skyboxTexture, skyboxMesh, shadowMapIndex, "Camera", nullptr );
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

            const teMesh* mesh = teMeshRendererGetMesh( scenes[ scene.index ].gameObjects[ gameObjectIndex ] );

            for (unsigned subMeshIndex = 0; subMeshIndex < teMeshGetSubMeshCount( mesh ); ++subMeshIndex)
            {
                Vec3 meshAabbMinWorld, meshAabbMaxWorld;
                teMeshGetSubMeshLocalAABB( *mesh, subMeshIndex, meshAabbMinWorld, meshAabbMaxWorld );

                // TODO: This could use the results of updateFrustumsAndCull() instead of calculating this again.
                Vec3 meshAabbWorld[ 8 ];
                teGetCorners( meshAabbMinWorld, meshAabbMaxWorld, meshAabbWorld );

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

void teSceneReadArraySizes( const teFile& sceneFile, unsigned& outGoCount, unsigned& outTextureCount, unsigned& outMaterialCount )
{
    char line[ 255 ] = {};

    unsigned i = 0;
    unsigned cursor = 0;
    int iter = 0;
    
    while (cursor < sceneFile.size)
    {        
        while (sceneFile.data[ i + cursor ] != '\n' && i + cursor < sceneFile.size)
        {
            line[ i ] = sceneFile.data[ i + cursor ];
            ++i;
        
            if (i == 256)
            {
                tePrint( "Scene file line is longer than 255 characters! Clamping to 255." );
            }
        }
        printf("line: %s\n", line);
        memset( line, 0, 255 );
        cursor += i;
        i = 0;
        //return;
        ++iter;
        if (iter > 20) return;
    }
}
