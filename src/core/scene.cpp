#include "scene.h"
#include "camera.h"
#include "file.h"
#include "frustum.h"
#include "gameobject.h"
#include "light.h"
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
#include <stdint.h>
#include <stdio.h>

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
void teAddLight( unsigned index );

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

static void RenderDepthAndNormals( unsigned cameraGOIndex, const teShader* shader )
{
    teClearFlag clearFlag;
    Vec4 clearColor;
    teCameraGetClear( cameraGOIndex, clearFlag, clearColor );

    teTexture2D& depthNormals = teCameraGetDepthNormalsTexture( cameraGOIndex );
    teTexture2D& depth = teCameraGetDepthTexture( cameraGOIndex );

    teAssert( depthNormals.index != 0 ); // Camera must have a render target!

    BeginRendering( depthNormals, depth, clearFlag, &clearColor.x );

    EndRendering( depthNormals );
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
static void RenderSceneWithCamera( const teScene& scene, unsigned cameraGOIndex, const teShader* skyboxShader, const teTextureCube* skyboxTexture, const teMesh* skyboxMesh, unsigned shadowMapindex, const char* profileMarker, const teShader* momentsShader, const teShader* depthNormalsShader )
{
    teTexture2D& color = teCameraGetColorTexture( cameraGOIndex );
    teTexture2D& depth = teCameraGetDepthTexture( cameraGOIndex );

    teAssert( color.index != 0 ); // Camera must have a render target!

    TransformSolveLocalMatrix( cameraGOIndex, true );
    UpdateFrustum( cameraGOIndex, teTransformGetLocalPosition( cameraGOIndex ), teTransformGetViewDirection( cameraGOIndex ) );
    UpdateTransformsAndCull( scene, cameraGOIndex );

    if (teCameraGetDepthNormalsTexture( cameraGOIndex ).index != 0 && depthNormalsShader)
    {
        RenderDepthAndNormals( cameraGOIndex, depthNormalsShader );
    }

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

        RenderSceneWithCamera( scene, index, nullptr, nullptr, nullptr, 0, "Shadow Map", &momentsShader, nullptr );
    }

    outShadowMapIndex = castShadowMap ? scenes[ scene.index ].shadowCaster.color.index : 0;
}

void teSceneRender( const teScene& scene, const teShader* skyboxShader, const teTextureCube* skyboxTexture, const teMesh* skyboxMesh, const teShader& momentsShader, const Vec3& dirLightPosition, const teShader& depthNormalsShader )
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
        RenderSceneWithCamera( scene, scenes[ scene.index ].gameObjects[ cameraIndex ], skyboxShader, skyboxTexture, skyboxMesh, shadowMapIndex, "Camera", nullptr, &depthNormalsShader );
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

void teSceneReadArraySizes( const teFile& sceneFile, unsigned& outGoCount, unsigned& outTextureCount, 
                            unsigned& outMaterialCount, unsigned& outMeshCount )
{
    outGoCount = 0;
    outTextureCount = 0;
    outMaterialCount = 0;
    outMeshCount = 0;

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
            printf( "line: %s\n", line );
            // TODO: make sure that file names containing spaces work.
            // TODO: don't add duplicates.
            if (teStrstr( line, "texture2d" ) == line)
            {
                ++outTextureCount;
            }
            else if (teStrstr( line, "material" ) == line)
            {
                ++outMaterialCount;
            }
            else if (teStrstr( line, "gameobject" ) == line)
            {
                ++outGoCount;
            }
            else if (teStrstr( line, "meshrenderer" ) == line)
            {
                //++outMeshCount;
            }
            else if (teStrstr( line, "meshmaterial" ) == line)
            {
                //++outMeshCount;
            }
            else if (teStrstr( line, "mesh" ) == line)
            {
                ++outMeshCount;
            }
            teZero( line, 255 );
        }

        ++cursor;
    }    
}

char gSceneStrings[ 20000 ];
uint32_t gNextFreeSceneString = 0;

uint32_t InsertSceneString( char* str )
{
    size_t len = teStrlen( str );
    teMemcpy( gSceneStrings + gNextFreeSceneString, str, len );
    uint32_t outIndex = gNextFreeSceneString;
    gNextFreeSceneString += (uint32_t)len + 1;
    teAssert( gNextFreeSceneString < 20000 );
    
    return outIndex;
}

void teSceneReadScene( const teFile& sceneFile, const teShader& standardShader, teGameObject* gos, teTexture2D* textures, teMaterial* materials, teMesh* meshes )
{
    unsigned goCount = 0;
    unsigned textureCount = 0;
    unsigned materialCount = 0;
    unsigned meshCount = 0;

    char line[ 255 ] = {};
    unsigned cursor = 0;
    unsigned i = 0;

    unsigned meshNameIndices[ 1000 ];
    unsigned textureNameIndices[ 1000 ];
    unsigned materialNameIndices[ 1000 ];

    unsigned tex0Index = 0;
    unsigned tex1Index = 0;

    while (cursor < sceneFile.size)
    {
        line[ i ] = sceneFile.data[ cursor ];
        ++i;

        if (sceneFile.data[ cursor ] == '\n')
        {
            line[ i - 1 ] = 0;
            i = 0;
            
            // TODO: make sure that file names containing spaces work.
            // TODO: don't add duplicates.
            if (teStrstr( line, "texture2d" ) == line)
            {
                char name[ 100 ] = {};
                unsigned nameCursor = 0;
                unsigned offset = teStrlen( "texture2d " );

                while (line[ nameCursor + offset ] != ' ')
                {
                    name[ nameCursor ] = line[ nameCursor + offset ];
                    ++nameCursor;
                }
                printf( "texture name: %s\n", name );
                textureNameIndices[ textureCount ] = InsertSceneString( name );

                char fileName[ 100 ] = {};
                unsigned fileNameCursor = 0;

                while (nameCursor + offset + fileNameCursor + 1 < teStrlen( line ) &&
                       line[ nameCursor + offset + fileNameCursor + 1 ] != '\r' && line[ nameCursor + offset + fileNameCursor + 1 ] != '\n')
                {
                    fileName[ fileNameCursor ] = line[ nameCursor + offset + fileNameCursor + 1 ];
                    ++fileNameCursor;
                }
                
                // TODO: if .dds is not supported by runtime, use .tga or .astc
                fileName[ fileNameCursor + 0 ] = '.';
                fileName[ fileNameCursor + 1 ] = 'd';
                fileName[ fileNameCursor + 2 ] = 'd';
                fileName[ fileNameCursor + 3 ] = 's';
                printf( "file name: %s\n", fileName );
                teFile texFile = teLoadFile( fileName );

                textures[ textureCount ] = teLoadTexture( texFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
                ++textureCount;
            }
            else if (teStrstr( line, "material" ) == line)
            {
                char name[ 100 ] = {};
                unsigned nameCursor = 0;
                unsigned offset = teStrlen( "material " );

                while (nameCursor + offset < teStrlen( line ) &&
                       line[ nameCursor + offset ] != '\r' && line[ nameCursor + offset ] != '\n')
                {
                    name[ nameCursor ] = line[ nameCursor + offset ];
                    ++nameCursor;
                }

                printf( "material name: %s\n", name );
                materialNameIndices[ materialCount ] = InsertSceneString( name );
                materials[ materialCount ] = teCreateMaterial( standardShader );

                ++materialCount;
            }
            else if (teStrstr( line, "gameobject" ) == line)
            {
                char name[ 100 ] = {};
                unsigned nameCursor = 0;
                unsigned offset = teStrlen( "gameobject " );

                while (nameCursor + offset < teStrlen( line ) &&
                       line[ nameCursor + offset ] != '\r' && line[ nameCursor + offset ] != '\n')
                {
                    name[ nameCursor ] = line[ nameCursor + offset ];
                    ++nameCursor;
                }
                printf( "gameobject name: %s\n", name );
                gos[ goCount ] = teCreateGameObject( "gameobject", teComponent::Transform );
                ++goCount;
            }
            else if (teStrstr( line, "meshmaterial" ) == line)
            {
                printf("line begins with meshmaterial\n");
                char index[ 100 ] = {};
                unsigned indexCursor = 0;
                unsigned offset = teStrlen( "meshmaterial " );

                while (line[ indexCursor + offset ] != ' ')
                {
                    index[ indexCursor ] = line[ indexCursor + offset ];
                    ++indexCursor;
                }
                printf( "meshmaterial submesh index: %s\n", index );

                char materialName[ 100 ] = {};
                unsigned materialNameCursor = 0;

                while (indexCursor + offset + materialNameCursor < teStrlen( line ) &&
                    line[ indexCursor + offset + materialNameCursor + 1 ] != '\r' && line[ indexCursor + offset + materialNameCursor + 1 ] != '\n')
                {
                    materialName[ materialNameCursor ] = line[ indexCursor + offset + materialNameCursor + 1 ];
                    ++materialNameCursor;
                }

                printf( "meshmaterial material name: %s\n", materialName );
                unsigned materialIndex = 0;

                for (unsigned m = 0; m < materialCount; ++m)
                {
                    if (teStrstr( gSceneStrings + meshNameIndices[ m ], materialName ))
                    {
                        materialIndex = m;
                    }
                }

                if (teStrstr( index, "all" ))
                {
                    printf( "all submeshes wanted\n" );
                    for (unsigned subMeshIndex = 0; subMeshIndex < teMeshGetSubMeshCount( teMeshRendererGetMesh( gos[ goCount - 1 ].index ) ); ++subMeshIndex)
                    {
                        teMeshRendererSetMaterial( gos[ goCount - 1 ].index, materials[ materialIndex ], subMeshIndex );
                    }
                }
                else
                {
                    unsigned subMeshIndex = atoi( index );
                    
                    if (subMeshIndex < teMeshGetSubMeshCount( teMeshRendererGetMesh( gos[ goCount - 1 ].index ) ))
                    {
                        teMeshRendererSetMaterial( gos[ goCount - 1 ].index, materials[ materialIndex ], subMeshIndex );
                    }
                }
            }
            else if (teStrstr( line, "meshrenderer" ) == line)
            {
                if (goCount == 0)
                {
                    printf( "meshrenderer without gameobject!\n" );
                }
                printf("line begins with meshrenderer\n");
                char name[ 100 ] = {};
                unsigned nameCursor = 0;
                unsigned offset = teStrlen( "meshrenderer " );

                while (nameCursor + offset < teStrlen( line ) &&
                       line[ nameCursor + offset ] != '\r' && line[ nameCursor + offset ] != '\n')
                {
                    name[ nameCursor ] = line[ nameCursor + offset ];
                    ++nameCursor;
                }
                printf( "meshrenderer for mesh: '%s'\n", name );
                teGameObjectAddComponent( gos[ goCount - 1 ].index, teComponent::MeshRenderer );

                bool found = false;

                for (unsigned m = 0; m < meshCount; ++m)
                {
                    if (!found && teStrstr( gSceneStrings + meshNameIndices[ m ], name ))
                    {
                        teMeshRendererSetMesh( gos[ goCount - 1 ].index, &meshes[ m ] );
                        found = true;
                    }
                }

                if (!found)
                {
                    printf( "meshrenderer: could not find '%s'\n", name );
                }
            }
            else if (teStrstr( line, "mesh" ) == line)
            {
                char name[ 100 ] = {};
                unsigned nameCursor = 0;
                unsigned offset = teStrlen( "mesh " );

                while (line[ nameCursor + offset ] != ' ')
                {
                    name[ nameCursor ] = line[ nameCursor + offset ];
                    ++nameCursor;
                }
                printf( "mesh name: %s\n", name );
                teAssert( meshCount < 1000 );
                meshNameIndices[ meshCount ] = InsertSceneString( name );

                char fileName[ 100 ] = {};
                unsigned fileNameCursor = 0;

                while (nameCursor + offset + fileNameCursor < teStrlen( line ) &&
                       line[ nameCursor + offset + fileNameCursor + 1 ] != '\r' && line[ nameCursor + offset + fileNameCursor + 1 ] != '\n')
                {
                    fileName[ fileNameCursor ] = line[ nameCursor + offset + fileNameCursor + 1 ];
                    ++fileNameCursor;
                }
                printf("mesh fileName: '%s'\n", fileName );
                teFile meshFile = teLoadFile( fileName );
                meshes[ meshCount ] = teLoadMesh( meshFile );
                ++meshCount;
            }
            else if (teStrstr( line, "tex0" ) == line)
            {
                char name[ 100 ] = {};
                unsigned nameCursor = 0;
                unsigned offset = teStrlen( "tex0 " );

                while (nameCursor + offset < teStrlen( line ) &&
                       line[ nameCursor + offset ] != '\r' && line[ nameCursor + offset ] != '\n')
                {
                    name[ nameCursor ] = line[ nameCursor + offset ];
                    ++nameCursor;
                }
                printf( "texture 0 name: %s\n", name );

                for (unsigned t = 0; t < textureCount; ++t)
                {
                    if (teStrstr( gSceneStrings + textureNameIndices[ t ], name ))
                    {
                        tex0Index = t;
                        teMaterialSetTexture2D( materials[ materialCount - 1 ], textures[ t ], 0 );
                        break;
                    }
                }
            }
            else if (teStrstr( line, "tex1" ) == line)
            {
                char name[ 100 ] = {};
                unsigned nameCursor = 0;
                unsigned offset = teStrlen( "tex1 " );
                
                while (nameCursor + offset < teStrlen( line ) &&
                       line[ nameCursor + offset ] != '\r' && line[ nameCursor + offset ] != '\n')
                {
                    name[ nameCursor ] = line[ nameCursor + offset ];
                    ++nameCursor;
                }
                printf( "texture 1 name: %s\n", name );

                for (unsigned t = 0; t < textureCount; ++t)
                {
                    if (teStrstr( gSceneStrings + textureNameIndices[ t ], name ))
                    {
                        tex1Index = t;
                        teMaterialSetTexture2D( materials[ materialCount - 1 ], textures[ t ], 1 );
                        break;
                    }
                }
            }
            else if (teStrstr( line, "lightcolor" ) == line)
            {
                printf( "TODO: read light color\n" );
            }
            else if (teStrstr( line, "light" ) == line)
            {
                char lightType[ 100 ] = {};
                unsigned typeCursor = 0;
                unsigned offset = teStrlen( "light " );
                
                while (typeCursor + offset < teStrlen( line ) &&
                       line[ typeCursor + offset ] != '\r' && line[ typeCursor + offset ] != '\n')
                {
                    lightType[ typeCursor ] = line[ typeCursor + offset ];
                    ++typeCursor;
                }
                printf( "light type: %s\n", lightType );
                teAddLight( gos[ goCount - 1 ].index );
            }
            
            teZero( line, 255 );
        }
        
        ++cursor;
    }
}
