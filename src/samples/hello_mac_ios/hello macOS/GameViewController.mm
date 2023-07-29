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

void InitApp( unsigned width, unsigned height );
void DrawApp();
void MoveForward( float amount );
void MoveUp( float amount );
void MoveRight( float amount );
void RotateCamera( float x, float y );
void MouseDown( int x, int y );
void MouseUp( int x, int y );
void MouseMove( int x, int y );
void SetDrawable( id< CAMetalDrawable > drawable );

extern MTLRenderPassDescriptor* renderPassDescriptor;
extern id<MTLCommandBuffer> gCommandBuffer;
NSViewController* myViewController;

@implementation GameViewController
{
    MTKView* _view;
    dispatch_semaphore_t inflightSemaphore;    
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
    inflightSemaphore = dispatch_semaphore_create( 2 );
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    dispatch_semaphore_wait( inflightSemaphore, DISPATCH_TIME_FOREVER );

    renderPassDescriptor = _view.currentRenderPassDescriptor;

    SetDrawable( view.currentDrawable );

    DrawApp();
        
    __block dispatch_semaphore_t block_sema = inflightSemaphore;
    [gCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) { dispatch_semaphore_signal( block_sema ); }];

    teEndFrame();
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    NSLog(@"Resize");
    
    const unsigned width = _view.bounds.size.width;
    const unsigned height = _view.bounds.size.height;
        
    InitApp( width, height );
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
    MouseDown( (int)theEvent.locationInWindow.x, (int)theEvent.locationInWindow.y );
}

- (void)mouseUp:(NSEvent *)theEvent
{
    MouseUp( (int)theEvent.locationInWindow.x, (int)theEvent.locationInWindow.y );
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    MouseMove( (int)theEvent.locationInWindow.x, self.view.bounds.size.height - (int)theEvent.locationInWindow.y );
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    RotateCamera( theEvent.deltaX, theEvent.deltaY );
    MouseMove( (int)theEvent.locationInWindow.x, self.view.bounds.size.height - (int)theEvent.locationInWindow.y );
}

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

@end
