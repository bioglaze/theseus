#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>
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

void SetDrawable( CA::MetalDrawable* drawable );
extern MTL::RenderPassDescriptor* renderPassDescriptor;
extern MTL::CommandBuffer* gCommandBuffer;

class MyMTKViewDelegate : public MTK::ViewDelegate
{
    public:
                     MyMTKViewDelegate( MTL::Device* pDevice );
        virtual      ~MyMTKViewDelegate() override;
        virtual void drawInMTKView( MTK::View* pView ) override;
        void         initSample( unsigned width, unsigned height );

    private:
        MTL::Device* m_device;
        MTL::CommandQueue* m_commandQueue;

        teShader      m_fullscreenShader;
        teShader      m_unlitShader;
        teShader      m_skyboxShader;
        teShader      m_momentsShader;
        teShader      m_standardShader;
        teMaterial    m_standardMaterial;
        teTexture2D   m_gliderTex;
        teTextureCube m_skyTex;
        teGameObject  m_camera3d;
        teGameObject  m_cubeGo;
        teScene       m_scene;
        teMesh        m_cubeMesh;
};

MyMTKViewDelegate::MyMTKViewDelegate( MTL::Device* pDevice )
: MTK::ViewDelegate()
{
    m_device = pDevice;
    m_commandQueue = m_device->newCommandQueue();
}

MyMTKViewDelegate::~MyMTKViewDelegate()
{
    m_commandQueue->release();
    m_device->release();
}

void MyMTKViewDelegate::initSample( unsigned width, unsigned height )
{
    m_fullscreenShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "fullscreenVS", "fullscreenPS" );
    m_unlitShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "unlitVS", "unlitPS" );
    m_skyboxShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "skyboxVS", "skyboxPS" );
    m_momentsShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "momentsVS", "momentsPS" );
    m_standardShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "standardVS", "standardPS" );

    m_camera3d = teCreateGameObject( "camera3d", teComponent::Transform | teComponent::Camera );
    Vec3 cameraPos = { 0, 0, -10 };
    teTransformSetLocalPosition( m_camera3d.index, cameraPos );
    teCameraSetProjection( m_camera3d.index, 45, width / (float)height, 0.1f, 800.0f );
    teCameraSetClear( m_camera3d.index, teClearFlag::DepthAndColor, Vec4( 1, 0, 0, 1 ) );
    teCameraGetColorTexture( m_camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::BGRA_sRGB, "camera3d color" );
    teCameraGetDepthTexture( m_camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::Depth32F, "camera3d depth" );

    teFile backFile   = teLoadFile( "assets/textures/skybox/back.dds" );
    teFile frontFile  = teLoadFile( "assets/textures/skybox/front.dds" );
    teFile leftFile   = teLoadFile( "assets/textures/skybox/left.dds" );
    teFile rightFile  = teLoadFile( "assets/textures/skybox/right.dds" );
    teFile topFile    = teLoadFile( "assets/textures/skybox/top.dds" );
    teFile bottomFile = teLoadFile( "assets/textures/skybox/bottom.dds" );

    m_skyTex = teLoadTexture( leftFile, rightFile, bottomFile, topFile, frontFile, backFile, 0 );
    m_gliderTex = teLoadTexture( teLoadFile( "assets/textures/glider_color.tga" ), teTextureFlags::GenerateMips, nullptr, 0, 0, teTextureFormat::Invalid );

    m_standardMaterial = teCreateMaterial( m_standardShader );
    teMaterialSetTexture2D( m_standardMaterial, m_gliderTex, 0 );

    m_cubeMesh = teCreateCubeMesh();
    m_cubeGo = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
    teMeshRendererSetMesh( m_cubeGo.index, &m_cubeMesh );
    teMeshRendererSetMaterial( m_cubeGo.index, m_standardMaterial, 0 );
    teTransformSetLocalScale( m_cubeGo.index, 4 );
    teTransformSetLocalPosition( m_cubeGo.index, Vec3( 0, -4, 0 ) );

    m_scene = teCreateScene( 0 );
    teSceneAdd( m_scene, m_camera3d.index );
    teSceneAdd( m_scene, m_cubeGo.index );

    teFinalizeMeshBuffers();
}

void MyMTKViewDelegate::drawInMTKView( MTK::View* pView )
{
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

    renderPassDescriptor = pView->currentRenderPassDescriptor();

    SetDrawable( pView->currentDrawable() );

    teBeginFrame();
    teSceneRender( m_scene, &m_skyboxShader, &m_skyTex, &m_cubeMesh, m_momentsShader, Vec3( 0, 0, -10 ) );
    teBeginSwapchainRendering();

    ShaderParams shaderParams;
    shaderParams.readTexture = teCameraGetColorTexture( m_camera3d.index ).index;
    shaderParams.tilesXY[ 0 ] = 2.0f;
    shaderParams.tilesXY[ 1 ] = 2.0f;
    shaderParams.tilesXY[ 2 ] = -1.0f;
    shaderParams.tilesXY[ 3 ] = -1.0f;
    teDrawQuad( m_fullscreenShader, teCameraGetColorTexture( m_camera3d.index ), shaderParams, teBlendMode::Off );
    teEndSwapchainRendering();
    teEndFrame();

    pPool->release();
}

class MyAppDelegate : public NS::ApplicationDelegate
{
    public:
        ~MyAppDelegate();

        virtual void applicationWillFinishLaunching( NS::Notification* pNotification ) override;
        virtual void applicationDidFinishLaunching( NS::Notification* pNotification ) override;
        virtual bool applicationShouldTerminateAfterLastWindowClosed( NS::Application* pSender ) override;

    private:
        MTK::View* m_view;
        MyMTKViewDelegate* m_viewDelegate = nullptr;
};

void MyAppDelegate::applicationWillFinishLaunching( NS::Notification* pNotification )
{
    NS::Application* app = reinterpret_cast< NS::Application* >( pNotification->object() );
    app->setActivationPolicy( NS::ActivationPolicy::ActivationPolicyRegular );
}

void MyAppDelegate::applicationDidFinishLaunching( NS::Notification* pNotification )
{
    //NS::Window* m_window = (NS::Window*)teCreateWindow( 512, 512, "mutsis" );
    
    CGRect frame = (CGRect){ {100.0, 100.0}, {512.0, 512.0} };

    NS::Window* m_window = NS::Window::alloc()->init(
    frame,
    NS::WindowStyleMaskClosable|NS::WindowStyleMaskTitled,
    NS::BackingStoreBuffered,
    false );
    
    MTL::Device* device = MTL::CreateSystemDefaultDevice();

    m_view = MTK::View::alloc()->init( frame, device );
    m_view->setColorPixelFormat( MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB );
    m_view->setClearColor( MTL::ClearColor::Make( 1.0, 0.0, 0.0, 1.0 ) );

    m_viewDelegate = new MyMTKViewDelegate( device );
    m_view->setDelegate( m_viewDelegate );

    m_window->setContentView( m_view );
    m_window->setTitle( NS::String::string( "Theseus Hello", NS::StringEncoding::UTF8StringEncoding ) );

    m_window->makeKeyAndOrderFront( nullptr );

    NS::Application* pApp = reinterpret_cast< NS::Application* >( pNotification->object() );
    pApp->activateIgnoringOtherApps( true );

    const unsigned width = frame.size.width;
    const unsigned height = frame.size.height;

    teCreateRenderer( 1, nullptr, width, height );
    teLoadMetalShaderLibrary();

    m_viewDelegate->initSample( width, height );
}

bool MyAppDelegate::applicationShouldTerminateAfterLastWindowClosed( NS::Application* pSender )
{
    return true;
}

MyAppDelegate::~MyAppDelegate()
{
    m_view->release();
    delete m_viewDelegate;
}

int main()
{
    NS::AutoreleasePool* autoreleasePool = NS::AutoreleasePool::alloc()->init();

    MyAppDelegate del;

    NS::Application* sharedApplication = NS::Application::sharedApplication();
    sharedApplication->setDelegate( &del );
    sharedApplication->run();

    autoreleasePool->release();
}
