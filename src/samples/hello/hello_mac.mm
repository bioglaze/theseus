#import <Metal/Metal.h>
#import <AppKit/AppKit.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
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

extern id<CAMetalDrawable> gDrawable;
extern MTLRenderPassDescriptor* renderPassDescriptor;
extern id<MTLCommandBuffer> gCommandBuffer;

unsigned width = 800, height = 450;

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

@interface HelloMetalView : MTKView
@end

@implementation HelloMetalView
- (id)initWithFrame:(CGRect)inFrame
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    self = [super initWithFrame:inFrame device:device];
    if (self)
    {
        teCreateRenderer( 1, nullptr, width, height );
        teLoadMetalShaderLibrary();

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
}
@end

@interface KeyHandlingWindow: NSWindow
@end

@implementation KeyHandlingWindow

- (void)keyDown:(NSEvent *)theEvent
{
    if ([theEvent keyCode] == 0x00) // A
    {
        MoveRight( 1 );
    }
    else if ([theEvent keyCode] == 0x02) // D
    {
        MoveRight( -1 );
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
        MoveUp( 1 );
    }
    else if ([theEvent keyCode] == 0x0E) // E
    {
        MoveUp( -1 );
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

- (void)mouseDown:(NSEvent *)theEvent
{
    //MouseDown( (int)theEvent.locationInWindow.x, (int)theEvent.locationInWindow.y );
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
