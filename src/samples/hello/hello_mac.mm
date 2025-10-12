#import <Metal/Metal.h>
#import <AppKit/AppKit.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include "camera.h"
#include "file.h"
#include "gameobject.h"
#include "light.h"
#include "math.h"
#include "mathutil.h"
#include "material.h"
#include "matrix.h"
#include "mesh.h"
#include "renderer.h"
#include "quaternion.h"
#include "scene.h"
#include "shader.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"

extern id<CAMetalDrawable> gDrawable;
extern MTLRenderPassDescriptor* renderPassDescriptor;
extern id<MTLCommandBuffer> gCommandBuffer;

unsigned width = 800, height = 450;

teShader      m_fullscreenShader;
teShader      m_fullscreenAdditiveShader;
teShader      m_unlitShader;
teShader      m_skyboxShader;
teShader      m_momentsShader;
teShader      m_standardShader;
teShader      m_bloomThresholdShader;
teShader      m_bloomBlurShader;
teShader      m_bloomCombineShader;
teShader      m_downsampleShader;
teShader      m_depthNormalsShader;
teShader      m_lightCullShader;
teMaterial    m_standardMaterial;
teMaterial    m_floorMaterial;
teMaterial    m_brickMaterial;
teTexture2D   m_gliderTex;
teTexture2D   m_brickTex;
teTexture2D   m_brickNormalTex;
teTexture2D   m_floorTex;
teTexture2D   m_floorNormalTex;
teTexture2D   m_bloomTarget;
teTexture2D   m_blurTarget;
teTexture2D   m_bloomComposeTarget;
teTexture2D   m_downsampleTarget;
teTexture2D   m_downsampleTarget2;
teTexture2D   m_downsampleTarget3;
teTexture2D   m_downsampleTestTarget;
teTexture2D   m_bilinearTestTex;
teTextureCube m_skyTex;
teGameObject  m_camera3d;
teGameObject  m_cubeGo;
teGameObject  m_roomGo;
teScene       m_scene;
teMesh        m_roomMesh;
teMesh        m_cubeMesh;
teGameObject pointLight;
teGameObject pointLight2;
Vec3 moveDir;

teTexture2D key0tex;
teTexture2D key1tex;
teTexture2D key2tex;
teTexture2D key3tex;
teTexture2D key4tex;
teTexture2D key5tex;
teTexture2D key6tex;
teTexture2D key7tex;
teTexture2D key8tex;
teTexture2D key9tex;

teMaterial key0mat;
teMaterial key1mat;
teMaterial key2mat;
teMaterial key3mat;
teMaterial key4mat;
teMaterial key5mat;
teMaterial key6mat;
teMaterial key7mat;
teMaterial key8mat;
teMaterial key9mat;

teMesh keypadMesh;
teGameObject keypadGo;
int activeDigit = 1;

void GetColliders( unsigned screenX, unsigned screenY, unsigned width, unsigned height, teScene scene, unsigned cameraIndex, int& outClosestSceneGo, unsigned& outClosestSubMesh )
{
    Vec3 rayOrigin, rayTarget;
    ScreenPointToRay( screenX, screenY, (float)width, (float)height, cameraIndex, rayOrigin, rayTarget );

    outClosestSceneGo = -1;
    float closestDistance = 99999.0f;
    outClosestSubMesh = 666;

    for (unsigned go = 0; go < teSceneGetMaxGameObjects(); ++go)
    {
        unsigned sceneGo = teSceneGetGameObjectIndex( scene, go );

        if ((teGameObjectGetComponents( sceneGo ) & teComponent::MeshRenderer) == 0)
        {
            continue;
        }

        for (unsigned subMesh = 0; subMesh < teMeshGetSubMeshCount( teMeshRendererGetMesh( sceneGo ) ); ++subMesh)
        {
            Vec3 mMinLocal, mMaxLocal;
            Vec3 mMinWorld, mMaxWorld;
            Vec3 mAABB[ 8 ];

            teMeshGetSubMeshLocalAABB( *teMeshRendererGetMesh( sceneGo ), subMesh, mMinLocal, mMaxLocal );
            teGetCorners( mMinLocal, mMaxLocal, mAABB );

            for (int v = 0; v < 8; ++v)
            {
                Matrix::TransformPoint( mAABB[ v ], teTransformGetMatrix( sceneGo ), mAABB[ v ] );
            }

            GetMinMax( mAABB, 8, mMinWorld, mMaxWorld );

            const float meshDistance = IntersectRayAABB( rayOrigin, rayTarget, mMinWorld, mMaxWorld );

            if (meshDistance > 0 && meshDistance < closestDistance)
            {
                closestDistance = meshDistance;
                outClosestSceneGo = sceneGo;
                outClosestSubMesh = subMesh;
            }
        }
    }
}

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

@interface HelloMetalView : MTKView
@end

@implementation HelloMetalView
- (id)initWithFrame:(CGRect)inFrame
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    self = [super initWithFrame:inFrame device:device];
    if (self)
    {
        self.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        self.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
        
        teCreateRenderer( 1, nullptr, width, height );
        teLoadMetalShaderLibrary();

        m_fullscreenShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "fullscreenVS", "fullscreenPS" );
        m_fullscreenAdditiveShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "fullscreenVS", "fullscreenAdditivePS" );
        m_unlitShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "unlitVS", "unlitPS" );
        m_skyboxShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "skyboxVS", "skyboxPS" );
        m_momentsShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "momentsVS", "momentsPS" );
        m_standardShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "standardVS", "standardPS" );
        m_bloomThresholdShader = teCreateComputeShader( teLoadFile( "" ), "bloomThreshold", 8, 8 );
        m_bloomBlurShader = teCreateComputeShader( teLoadFile( "" ), "bloomBlur", 8, 8 );
        m_bloomCombineShader = teCreateComputeShader( teLoadFile( "" ), "bloomCombine", 8, 8 );
        m_downsampleShader = teCreateComputeShader( teLoadFile( "" ), "bloomDownsample", 8, 8 );
        m_depthNormalsShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "depthNormalsVS", "depthNormalsPS" );
        m_lightCullShader = teCreateComputeShader( teLoadFile( "" ), "cullLights", 16, 16 );

        m_camera3d = teCreateGameObject( "camera3d", teComponent::Transform | teComponent::Camera );
        Vec3 cameraPos = { 0, 0, 10 };
        teTransformSetLocalPosition( m_camera3d.index, cameraPos );
        teCameraSetProjection( m_camera3d.index, 45, width / (float)height, 0.1f, 800.0f );
        teCameraSetClear( m_camera3d.index, teClearFlag::DepthAndColor, Vec4( 1, 0, 0, 1 ) );
        teCameraGetColorTexture( m_camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::BGRA_sRGB, "camera3d color" );
        teCameraGetDepthTexture( m_camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::Depth32F, "camera3d depth" );
        teCameraGetDepthNormalsTexture( m_camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::R32G32B32A32F, "camera3d depthNormals" );
        m_bloomTarget = teCreateTexture2D( width, height, teTextureFlags::UAV, teTextureFormat::R32F, "bloomTarget" );
        m_blurTarget = teCreateTexture2D( width, height, teTextureFlags::UAV, teTextureFormat::R32F, "blurTarget" );
        m_bloomComposeTarget = teCreateTexture2D( width, height, teTextureFlags::UAV, teTextureFormat::R32F, "bloomComposeTarget" );
        m_downsampleTarget = teCreateTexture2D( width / 2, height / 2, teTextureFlags::UAV, teTextureFormat::R32F, "downsampleTarget" );
        m_downsampleTarget2 = teCreateTexture2D( width / 4, height / 4, teTextureFlags::UAV, teTextureFormat::R32F, "downsampleTarget2" );
        m_downsampleTarget3 = teCreateTexture2D( width / 8, height / 8, teTextureFlags::UAV, teTextureFormat::R32F, "downsampleTarget3" );

        m_downsampleTestTarget = teCreateTexture2D( 8, 8, teTextureFlags::UAV, teTextureFormat::RGBA_sRGB, "downsampleTestTarget" );

        teFile backFile   = teLoadFile( "assets/textures/skybox/back.dds" );
        teFile frontFile  = teLoadFile( "assets/textures/skybox/front.dds" );
        teFile leftFile   = teLoadFile( "assets/textures/skybox/left.dds" );
        teFile rightFile  = teLoadFile( "assets/textures/skybox/right.dds" );
        teFile topFile    = teLoadFile( "assets/textures/skybox/top.dds" );
        teFile bottomFile = teLoadFile( "assets/textures/skybox/bottom.dds" );

        m_skyTex = teLoadTexture( leftFile, rightFile, bottomFile, topFile, frontFile, backFile, 0 );
        m_gliderTex = teLoadTexture( teLoadFile( "assets/textures/glider_color.tga" ), teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

        teFile brickFile = teLoadFile( "assets/textures/brickwall_d.dds" );
        m_brickTex = teLoadTexture( brickFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

        teFile bilinearFile = teLoadFile( "assets/textures/test/bilinear_test.tga" );
        m_bilinearTestTex = teLoadTexture( bilinearFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

        teFile brickNormalFile = teLoadFile( "assets/textures/brickwall_n.dds" );
        m_brickNormalTex = teLoadTexture( brickNormalFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

        teFile floorFile = teLoadFile( "assets/textures/plaster_d.dds" );
        m_floorTex = teLoadTexture( floorFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

        teFile floorNormalFile = teLoadFile( "assets/textures/plaster_n.dds" );
        m_floorNormalTex = teLoadTexture( floorNormalFile, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

        m_standardMaterial = teCreateMaterial( m_standardShader );
        teMaterialSetTexture2D( m_standardMaterial, m_gliderTex, 0 );
        teMaterialSetTexture2D( m_standardMaterial, m_floorNormalTex, 1 );

        m_floorMaterial = teCreateMaterial( m_standardShader );
        teMaterialSetTexture2D( m_floorMaterial, m_floorTex, 0 );
        teMaterialSetTexture2D( m_floorMaterial, m_floorNormalTex, 1 );

        m_brickMaterial = teCreateMaterial( m_standardShader );
        teMaterialSetTexture2D( m_brickMaterial, m_brickTex, 0 );
        teMaterialSetTexture2D( m_brickMaterial, m_floorNormalTex, 1 );

        teFile roomFile = teLoadFile( "assets/meshes/room.t3d" );
        m_roomMesh = teLoadMesh( roomFile );
        m_roomGo = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
        teMeshRendererSetMesh( m_roomGo.index, &m_roomMesh );
        teMeshRendererSetMaterial( m_roomGo.index, m_floorMaterial, 0 );
        teMeshRendererSetMaterial( m_roomGo.index, m_brickMaterial, 1 );
        teMeshRendererSetMaterial( m_roomGo.index, m_brickMaterial, 2 );
        teMeshRendererSetMaterial( m_roomGo.index, m_brickMaterial, 3 );
        teMeshRendererSetMaterial( m_roomGo.index, m_brickMaterial, 4 );
        teMeshRendererSetMaterial( m_roomGo.index, m_brickMaterial, 5 );

        m_cubeMesh = teCreateCubeMesh();
        m_cubeGo = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
        teMeshRendererSetMesh( m_cubeGo.index, &m_cubeMesh );
        teMeshRendererSetMaterial( m_cubeGo.index, m_standardMaterial, 0 );
        //teTransformSetLocalScale( m_cubeGo.index, 4 );
        teTransformSetLocalPosition( m_cubeGo.index, Vec3( 0, 4, 0 ) );

        teFile key0File = teLoadFile( "assets/textures/digits/zero.tga" );
        teFile key1File = teLoadFile( "assets/textures/digits/one.tga" );
        teFile key2File = teLoadFile( "assets/textures/digits/two.tga" );
        teFile key3File = teLoadFile( "assets/textures/digits/three.tga" );
        teFile key4File = teLoadFile( "assets/textures/digits/four.tga" );
        teFile key5File = teLoadFile( "assets/textures/digits/five.tga" );
        teFile key6File = teLoadFile( "assets/textures/digits/six.tga" );
        teFile key7File = teLoadFile( "assets/textures/digits/seven.tga" );
        teFile key8File = teLoadFile( "assets/textures/digits/eight.tga" );
        teFile key9File = teLoadFile( "assets/textures/digits/nine.tga" );
        
        key0tex = teLoadTexture( key0File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
        key1tex = teLoadTexture( key1File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
        key2tex = teLoadTexture( key2File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
        key3tex = teLoadTexture( key3File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
        key4tex = teLoadTexture( key4File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
        key5tex = teLoadTexture( key5File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
        key6tex = teLoadTexture( key6File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
        key7tex = teLoadTexture( key7File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
        key8tex = teLoadTexture( key8File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );
        key9tex = teLoadTexture( key9File, teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

        key0mat = teCreateMaterial( m_standardShader );
        teMaterialSetTexture2D( key0mat, key0tex, 0 );
        teMaterialSetTexture2D( key0mat, m_floorNormalTex, 1 );

        key1mat = teCreateMaterial( m_standardShader );
        teMaterialSetTexture2D( key1mat, key1tex, 0 );
        teMaterialSetTexture2D( key1mat, m_floorNormalTex, 1 );

        key2mat = teCreateMaterial( m_standardShader );
        teMaterialSetTexture2D( key2mat, key2tex, 0 );
        teMaterialSetTexture2D( key2mat, m_floorNormalTex, 1 );

        key3mat = teCreateMaterial( m_standardShader );
        teMaterialSetTexture2D( key3mat, key3tex, 0 );
        teMaterialSetTexture2D( key3mat, m_floorNormalTex, 1 );

        key4mat = teCreateMaterial( m_standardShader );
        teMaterialSetTexture2D( key4mat, key4tex, 0 );
        teMaterialSetTexture2D( key4mat, m_floorNormalTex, 1 );

        key5mat = teCreateMaterial( m_standardShader );
        teMaterialSetTexture2D( key5mat, key5tex, 0 );
        teMaterialSetTexture2D( key5mat, m_floorNormalTex, 1 );

        key6mat = teCreateMaterial( m_standardShader );
        teMaterialSetTexture2D( key6mat, key6tex, 0 );
        teMaterialSetTexture2D( key6mat, m_floorNormalTex, 1 );

        key7mat = teCreateMaterial( m_standardShader );
        teMaterialSetTexture2D( key7mat, key7tex, 0 );
        teMaterialSetTexture2D( key7mat, m_floorNormalTex, 1 );

        key8mat = teCreateMaterial( m_standardShader );
        teMaterialSetTexture2D( key8mat, key8tex, 0 );
        teMaterialSetTexture2D( key8mat, m_floorNormalTex, 1 );

        key9mat = teCreateMaterial( m_standardShader );
        teMaterialSetTexture2D( key9mat, key9tex, 0 );
        teMaterialSetTexture2D( key9mat, m_floorNormalTex, 1 );

        teFile keypadFile = teLoadFile( "assets/meshes/keypad.t3d" );
        keypadMesh = teLoadMesh( keypadFile );

        keypadGo = teCreateGameObject( "keypad", teComponent::Transform | teComponent::MeshRenderer );
        Vec3 keypadPos = Vec3( -10, 4, 3 );
        teTransformSetLocalPosition( keypadGo.index, keypadPos );
        teMeshRendererSetMesh( keypadGo.index, &keypadMesh );
        teMeshRendererSetMaterial( keypadGo.index, key0mat, 0 );
        teMeshRendererSetMaterial( keypadGo.index, key0mat, 1 );
        teMeshRendererSetMaterial( keypadGo.index, key0mat, 2 );
        teMeshRendererSetMaterial( keypadGo.index, key0mat, 3 );
        teMeshRendererSetMaterial( keypadGo.index, key0mat, 4 );
        teMeshRendererSetMaterial( keypadGo.index, key1mat, 5 );
        teMeshRendererSetMaterial( keypadGo.index, key2mat, 6 );
        teMeshRendererSetMaterial( keypadGo.index, key3mat, 7 );
        teMeshRendererSetMaterial( keypadGo.index, key4mat, 8 );
        teMeshRendererSetMaterial( keypadGo.index, key5mat, 9 );
        teMeshRendererSetMaterial( keypadGo.index, key6mat, 10 );
        teMeshRendererSetMaterial( keypadGo.index, key7mat, 11 );
        teMeshRendererSetMaterial( keypadGo.index, key8mat, 12 );
        teMeshRendererSetMaterial( keypadGo.index, key9mat, 13 );
        teMeshRendererSetMaterial( keypadGo.index, key0mat, 14 );
        teMeshRendererSetMaterial( keypadGo.index, key0mat, 15 );
        teMeshRendererSetMaterial( keypadGo.index, key0mat, 16 );
        teMeshRendererSetMaterial( keypadGo.index, key0mat, 17 );

        teFile sceneFile = teLoadFile( "assets/hello.tscene" );
        teGameObject* sceneGos;
        teTexture2D* sceneTextures;
        teMaterial* sceneMaterials;
        teMesh* sceneMeshes;
        unsigned sceneGoCount = 0;
        unsigned sceneTextureCount = 0;
        unsigned sceneMaterialCount = 0;
        unsigned sceneMeshCount = 0;
        teSceneReadArraySizes( sceneFile, sceneGoCount, sceneTextureCount, sceneMaterialCount, sceneMeshCount );
        sceneGos = (teGameObject*)malloc( sceneGoCount * sizeof( teGameObject ) );
        sceneTextures = (teTexture2D*)malloc( sceneTextureCount * sizeof( teTexture2D ) );
        sceneMaterials = (teMaterial*)malloc( sceneMaterialCount * sizeof( teMaterial ) );
        sceneMeshes = (teMesh*)malloc( sceneMeshCount * sizeof( teMesh ) );
        printf( "sceneGos: %u, sceneTextures: %u, sceneMaterials: %u, sceneMeshes: %u\n",
            sceneGoCount, sceneTextureCount, sceneMaterialCount, sceneMeshCount );
        teSceneReadScene( sceneFile, m_standardShader, sceneGos, sceneTextures, sceneMaterials, sceneMeshes );

        Vec3 lightPos = Vec3( -10, 1, 3 );
        pointLight = teCreateGameObject( "cube2", teComponent::Transform | teComponent::PointLight );
        teTransformSetLocalPosition( pointLight.index, lightPos );
        tePointLightSetParams( pointLight.index, 3, { 1, 0, 0 } );

        Vec3 light2Pos = Vec3( -13, 1, 3 );
        pointLight2 = teCreateGameObject( "cube2", teComponent::Transform | teComponent::PointLight );
        teTransformSetLocalPosition( pointLight2.index, light2Pos );
        tePointLightSetParams( pointLight2.index, 3, { 0, 1, 0 } );

        m_scene = teCreateScene( 2048 );
        teSceneAdd( m_scene, m_camera3d.index );
        teSceneAdd( m_scene, m_cubeGo.index );
        //teSceneAdd( m_scene, m_roomGo.index );
        teSceneAdd( m_scene, keypadGo.index );
        teSceneAdd( m_scene, pointLight.index );
        teSceneAdd( m_scene, pointLight2.index );

        for (unsigned i = 0; i < sceneGoCount; ++i)
        {
            teSceneAdd( m_scene, sceneGos[ i ].index );
        }

        teSceneSetupDirectionalLight( m_scene, Vec3( 1, 1, 1 ), Vec3( 0.005f, -1, 0.005f ).Normalized() );

        teFinalizeMeshBuffers();
    }
    return self;
}

- (void)drawRect:(CGRect)rect
{
    float dt = 1.0f;
    teTransformMoveForward( m_camera3d.index, moveDir.z * (float)dt * 0.5f, false, false, false );
    teTransformMoveRight( m_camera3d.index, moveDir.x * (float)dt * 0.5f );
    teTransformMoveUp( m_camera3d.index, moveDir.y * (float)dt * 0.5f );

    renderPassDescriptor = self.currentRenderPassDescriptor;

    gDrawable = self.currentDrawable;
    teBeginFrame();
    Vec3 cubePos = Vec3( 0, 45, 0 );
    const Vec3 dirLightShadowCasterPosition = cubePos;
    teSceneRender( m_scene, &m_skyboxShader, &m_skyTex, &m_cubeMesh, m_momentsShader, dirLightShadowCasterPosition, m_depthNormalsShader, m_lightCullShader );

    ShaderParams shaderParams = {};
    shaderParams.tint[ 0 ] = 0.6f;
    shaderParams.tint[ 1 ] = 0.5f;
    shaderParams.tint[ 2 ] = 0.4f;
    shaderParams.tint[ 3 ] = 0.3f;

#if 1
    float bloomThreshold = 0.1f;

    shaderParams.readTexture = teCameraGetColorTexture( m_camera3d.index ).index;
    shaderParams.writeTexture = m_bloomTarget.index;
    shaderParams.bloomThreshold = pow( 2, bloomThreshold ) - 1;
    teShaderDispatch( m_bloomThresholdShader, width / 8, height / 8, 1, shaderParams, "bloom threshold" );

    // TODO UAV barrier here
    shaderParams.readTexture = m_bloomTarget.index;
    shaderParams.writeTexture = m_blurTarget.index;
    shaderParams.tilesXY[ 2 ] = 1.0f;
    shaderParams.tilesXY[ 3 ] = 0.0f;
    teShaderDispatch( m_bloomBlurShader, width / 8, height / 8, 1, shaderParams, "bloom blur h" );

    // TODO UAV barrier here
    shaderParams.readTexture = m_blurTarget.index;
    shaderParams.writeTexture = m_bloomTarget.index;
    shaderParams.tilesXY[ 2 ] = 0.0f;
    shaderParams.tilesXY[ 3 ] = 1.0f;
    teShaderDispatch( m_bloomBlurShader, width / 8, height / 8, 1, shaderParams, "bloom blur v" );

    // additional blurring
    shaderParams.readTexture = m_bloomTarget.index;
    shaderParams.writeTexture = m_blurTarget.index;
    shaderParams.tilesXY[ 2 ] = 1.0f;
    shaderParams.tilesXY[ 3 ] = 0.0f;
    teShaderDispatch( m_bloomBlurShader, width / 8, height / 8, 1, shaderParams, "bloom blur h 2" );

    shaderParams.readTexture = m_blurTarget.index;
    shaderParams.writeTexture = m_bloomTarget.index;
    shaderParams.tilesXY[ 2 ] = 0.0f;
    shaderParams.tilesXY[ 3 ] = 1.0f;
    teShaderDispatch( m_bloomBlurShader, width / 8, height / 8, 1, shaderParams, "bloom blur v 2" );

    // additional blurring 2
    shaderParams.readTexture = m_bloomTarget.index;
    shaderParams.writeTexture = m_blurTarget.index;
    shaderParams.tilesXY[ 2 ] = 1.0f;
    shaderParams.tilesXY[ 3 ] = 0.0f;
    teShaderDispatch( m_bloomBlurShader, width / 8, height / 8, 1, shaderParams, "bloom blur h 3" );

    shaderParams.readTexture = m_blurTarget.index;
    shaderParams.writeTexture = m_bloomTarget.index;
    shaderParams.tilesXY[ 2 ] = 0.0f;
    shaderParams.tilesXY[ 3 ] = 1.0f;
    teShaderDispatch( m_bloomBlurShader, width / 8, height / 8, 1, shaderParams, "bloom blur v 3" );

    // Downsample init.
    shaderParams.tilesXY[ 2 ] = 1.0f;
    shaderParams.tilesXY[ 3 ] = 1.0f;

    // Downsample 1
    shaderParams.readTexture = m_bloomTarget.index;
    shaderParams.writeTexture = m_downsampleTarget.index;
    shaderParams.tilesXY[ 0 ] = width / 2;
    shaderParams.tilesXY[ 1 ] = height / 2;
    teShaderDispatch( m_downsampleShader, width / 8, height / 8, 1, shaderParams, "bloom downsample 1" );

    // Downsample 2
    shaderParams.readTexture = m_downsampleTarget.index;
    shaderParams.writeTexture = m_downsampleTarget2.index;
    shaderParams.tilesXY[ 0 ] = width / 4;
    shaderParams.tilesXY[ 1 ] = height / 4;
    teShaderDispatch( m_downsampleShader, width / 8, height / 8, 1, shaderParams, "bloom downsample 2" );

    // Downsample 3
    shaderParams.readTexture = m_downsampleTarget2.index;
    shaderParams.writeTexture = m_downsampleTarget3.index;
    shaderParams.tilesXY[ 0 ] = width / 8;
    shaderParams.tilesXY[ 1 ] = height / 8;
    teShaderDispatch( m_downsampleShader, width / 8, height / 8, 1, shaderParams, "bloom downsample 3" );

    // Combine
    shaderParams.readTexture = m_bloomTarget.index;
    shaderParams.readTexture2 = m_downsampleTarget.index;
    shaderParams.readTexture3 = m_downsampleTarget2.index;
    shaderParams.readTexture4 = m_downsampleTarget3.index;
    shaderParams.writeTexture = m_bloomComposeTarget.index;
    shaderParams.tilesXY[ 0 ] = width / 2;
    shaderParams.tilesXY[ 1 ] = height / 2;
    teShaderDispatch( m_bloomCombineShader, width / 8, height / 8, 1, shaderParams, "bloom combine" );
#endif

    // Bilinear test
    /*shaderParams.readTexture = m_bilinearTestTex.index;
    shaderParams.writeTexture = m_downsampleTestTarget.index;
    shaderParams.tilesXY[ 0 ] = 16;
    shaderParams.tilesXY[ 1 ] = 16;
    teShaderDispatch( m_downsampleShader, 16 / 8, 16 / 8, 1, shaderParams, "downsample test" );*/

    teBeginSwapchainRendering();

    shaderParams.readTexture = teCameraGetColorTexture( m_camera3d.index ).index;
    shaderParams.tilesXY[ 0 ] = 2.0f;
    shaderParams.tilesXY[ 1 ] = 2.0f;
    shaderParams.tilesXY[ 2 ] = -1.0f;
    shaderParams.tilesXY[ 3 ] = -1.0f;

    teDrawQuad( m_fullscreenShader, teCameraGetColorTexture( m_camera3d.index ), shaderParams, teBlendMode::Off );

    shaderParams.tilesXY[ 0 ] = 8.0f; // FIXME: Why is 8 needed here? In hello.cpp it's 4.
    shaderParams.tilesXY[ 1 ] = 8.0f;
    teDrawQuad( m_fullscreenAdditiveShader, m_bloomComposeTarget, shaderParams, teBlendMode::Additive );
    //teDrawQuad( m_fullscreenAdditiveShader, m_downsampleTestTarget, shaderParams, teBlendMode::Additive );

    teEndSwapchainRendering();
    teEndFrame();
}
@end

@interface KeyHandlingWindow: NSWindow
@end

@implementation KeyHandlingWindow

- (void)keyDown:(NSEvent *)theEvent
{
    if ([theEvent keyCode] == 0x00) // A
    {
        MoveRight( -1 );
    }
    else if ([theEvent keyCode] == 0x02) // D
    {
        MoveRight( 1 );
    }
    else if ([theEvent keyCode] == 0x0D) // W
    {
        MoveForward( 1 );
    }
    else if ([theEvent keyCode] == 0x01) // S
    {
        MoveForward( -1 );
    }
    else if ([theEvent keyCode] == 0x0C) // Q
    {
        MoveUp( -1 );
    }
    else if ([theEvent keyCode] == 0x0E) // E
    {
        MoveUp( 1 );
    }
}

- (void)keyUp:(NSEvent *)theEvent
{
    if ([theEvent keyCode] == 0x00) // A
    {
        MoveRight( 0 );
    }
    else if ([theEvent keyCode] == 0x02) // D
    {
        MoveRight( 0 );
    }
    else if ([theEvent keyCode] == 0x0D) // W
    {
        MoveForward( 0 );
    }
    else if ([theEvent keyCode] == 0x01) // S
    {
        MoveForward( 0 );
    }
    else if ([theEvent keyCode] == 0x0C) // Q
    {
        MoveUp( 0 );
    }
    else if ([theEvent keyCode] == 0x0E) // E
    {
        MoveUp( 0 );
    }
}

- (void)mouseDown:(NSEvent *)event
{
    //MouseDown( (int)event.locationInWindow.x, (int)event.locationInWindow.y );
    int x = (int)event.locationInWindow.x;
    int y = height - (int)event.locationInWindow.y;

    int closestSceneGo = 0;
    unsigned closestSubMesh = 0;
    GetColliders( x, y, width, height, m_scene, m_camera3d.index, closestSceneGo, closestSubMesh );
    //printf( "closest go: %d, closest submesh: %u\n", closestSceneGo, closestSubMesh );
    // display digit submeshes: 1-4
    // number pad: 5-14
    if (closestSceneGo == keypadGo.index)
    {
        if (closestSubMesh >= 5 && closestSubMesh < 15)
        {
            if (closestSubMesh == 5) teMeshRendererSetMaterial( keypadGo.index, key1mat, activeDigit );
            if (closestSubMesh == 6) teMeshRendererSetMaterial( keypadGo.index, key2mat, activeDigit );
            if (closestSubMesh == 7) teMeshRendererSetMaterial( keypadGo.index, key3mat, activeDigit );
            if (closestSubMesh == 8) teMeshRendererSetMaterial( keypadGo.index, key4mat, activeDigit );
            if (closestSubMesh == 9) teMeshRendererSetMaterial( keypadGo.index, key5mat, activeDigit );
            if (closestSubMesh == 10) teMeshRendererSetMaterial( keypadGo.index, key6mat, activeDigit );
            if (closestSubMesh == 11) teMeshRendererSetMaterial( keypadGo.index, key7mat, activeDigit );
            if (closestSubMesh == 12) teMeshRendererSetMaterial( keypadGo.index, key8mat, activeDigit );
            if (closestSubMesh == 13) teMeshRendererSetMaterial( keypadGo.index, key9mat, activeDigit );
            if (closestSubMesh == 14) teMeshRendererSetMaterial( keypadGo.index, key0mat, activeDigit );

            ++activeDigit;
            if (activeDigit == 5)
            {
                activeDigit = 1;
            }
        }
    }
}

- (void)mouseUp:(NSEvent *)theEvent
{
    //MouseUp( (int)theEvent.locationInWindow.x, (int)theEvent.locationInWindow.y );
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    //MouseMove( (int)theEvent.locationInWindow.x, self.view.bounds.size.height - (int)theEvent.locationInWindow.y );
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    //RotateCamera( theEvent.deltaX, theEvent.deltaY );
    teTransformOffsetRotate( m_camera3d.index, Vec3( 0, 1, 0 ), -theEvent.deltaX / 20.0f );
    teTransformOffsetRotate( m_camera3d.index, Vec3( 1, 0, 0 ), -theEvent.deltaY / 20.0f );

    //MouseMove( (int)theEvent.locationInWindow.x, self.view.bounds.size.height - (int)theEvent.locationInWindow.y );
}

@end

int main()
{
    @autoreleasepool
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp activateIgnoringOtherApps:YES];
        
        // Menu.
        NSMenu* bar = [NSMenu new];
        NSMenuItem * barItem = [NSMenuItem new];
        NSMenu* menu = [NSMenu new];
        NSMenuItem* quit = [[NSMenuItem alloc]
                            initWithTitle:@"Quit"
                            action:@selector(terminate:)
                            keyEquivalent:@"q"];
        [bar addItem:barItem];
        [barItem setSubmenu:menu];
        [menu addItem:quit];
        NSApp.mainMenu = bar;
        
        NSRect rect = NSMakeRect(0, 0, width, height);
        NSRect frame = NSMakeRect(0, 0, width, height);
        NSWindow* window = [[KeyHandlingWindow alloc]
                            initWithContentRect:rect
                            styleMask:NSWindowStyleMaskTitled
                            backing:NSBackingStoreBuffered
                            defer:NO];
        [window cascadeTopLeftFromPoint:NSMakePoint(20,20)];
        window.styleMask |= NSWindowStyleMaskResizable;
        window.styleMask |= NSWindowStyleMaskMiniaturizable ;
        window.styleMask |= NSWindowStyleMaskClosable;
        window.title = [[NSProcessInfo processInfo] processName];
        [window makeKeyAndOrderFront:nil];
        
        HelloMetalView* view = [[HelloMetalView alloc] initWithFrame:frame];
        window.contentView = view;
        
        [NSApp run];
        NSLog(@"run()");
    }
    
    return 0;
}
