#import "GameViewController.h"
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

void SetDrawable( id< CAMetalDrawable > drawable );
NSViewController* myViewController;

@implementation GameViewController
{
    MTKView *_view;

    teShader unlitShader;
    teShader fullscreenShader;
    
    teGameObject camera3d;
    teGameObject cubeGo;
    teMaterial material;
    
    teScene scene;
}

void RotateCamera( unsigned index, float x, float y )
{
    teTransformOffsetRotate( index, Vec3( 0, 1, 0 ), -x / 20 );
    teTransformOffsetRotate( index, Vec3( 1, 0, 0 ), -y / 20 );
}

void MoveForward( float amount )
{
    //app.moveDir.z = 0.3f * amount;
}

void MoveRight( float amount )
{
    //app.moveDir.x = 0.3f * amount;
}

void MoveUp( float amount )
{
    //app.moveDir.y = 0.3f * amount;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _view = (MTKView *)self.view;

    _view.device = MTLCreateSystemDefaultDevice();

    if(!_view.device)
    {
        NSLog(@"Metal is not supported on this device");
        self.view = [[NSView alloc] initWithFrame:self.view.frame];
        return;
    }

    _view = (MTKView *)self.view;
    _view.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    _view.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
    _view.sampleCount = 1;
    _view.delegate = self;
    
    myViewController = self;
    
    const unsigned width = self.view.bounds.size.width;
    const unsigned height = self.view.bounds.size.height;
    
    teCreateRenderer( 1, NULL, width, height );
    
    fullscreenShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "fullscreenVS", "fullscreenPS" );
    unlitShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "unlitVS", "unlitPS" );
    
    camera3d = teCreateGameObject( "camera3d", teComponent::Transform | teComponent::Camera );
    Vec3 cameraPos = { 0, 0, -10 };
    teTransformSetLocalPosition( camera3d.index, cameraPos );
    teCameraSetProjection( camera3d.index, 45, width / (float)height, 0.1f, 800.0f );

    teCameraGetColorTexture( camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::BGRA_sRGB, "camera3d color" );
    teCameraGetDepthTexture( camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::Depth32F, "camera3d depth" );
    
    teMesh cubeMesh = teCreateCubeMesh();
    cubeGo = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
    teMeshRendererSetMesh( cubeGo.index, &cubeMesh );
    teMeshRendererSetMaterial( cubeGo.index, material, 0 );

    scene = teCreateScene();
    teSceneAdd( scene, camera3d.index );
    teSceneAdd( scene, cubeGo.index );
    
    teFinalizeMeshBuffers();
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    SetDrawable( view.currentDrawable );
    teBeginFrame();
    teSceneRender( scene, NULL, NULL, NULL );
    teBeginSwapchainRendering( teCameraGetColorTexture( camera3d.index ) );
    teDrawFullscreenTriangle( fullscreenShader, teCameraGetColorTexture( camera3d.index ) );
    teEndSwapchainRendering();
    teEndFrame();
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    
}

- (BOOL)commitEditingAndReturnError:(NSError *__autoreleasing  _Nullable * _Nullable)error
{
    return TRUE;
}

- (void)encodeWithCoder:(nonnull NSCoder *)coder
{
    
}

- (void)mouseDown:(NSEvent *)theEvent
{
    NSLog(@"mouse down\n");
    //inputEvent.x = (int)theEvent.locationInWindow.x;
}

- (void)mouseUp:(NSEvent *)theEvent
{
    NSLog(@"mouse up\n");
    //inputEvent.x = (int)theEvent.locationInWindow.x;
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    RotateCamera( camera3d.index, theEvent.deltaX, theEvent.deltaY );
}

- (void)keyDown:(NSEvent *)theEvent
{
    NSLog(@"key down\n");
    
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

@end
