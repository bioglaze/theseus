#import "GameViewController.h"
#import "RRenderer.h"
#include "camera.h"
#include "file.h"
#include "gameobject.h"
#include "imgui.h"
#include "material.h"
#include "mesh.h"
#include "renderer.h"
#include "quaternion.h"
#include "scene.h"
#include "shader.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"
#include <stdint.h>

void InitSceneView( unsigned width, unsigned height, void* windowHandle, int uiScale );
NSViewController* myViewController;

@implementation GameViewController
{
    MTKView* _view;

    Renderer* _renderer;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _view = (MTKView *)self.view;
    _view.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    _view.depthStencilPixelFormat = MTLPixelFormatDepth32Float;

    _view.device = MTLCreateSystemDefaultDevice();

    if(!_view.device)
    {
        NSLog(@"Metal is not supported on this device");
        self.view = [[NSView alloc] initWithFrame:self.view.frame];
        return;
    }

    _renderer = [[Renderer alloc] initWithMetalKitView:_view];

    [_renderer mtkView:_view drawableSizeWillChange:_view.drawableSize];

    _view.delegate = _renderer;

    const unsigned width = _view.bounds.size.width;
    const unsigned height = _view.bounds.size.height;
    InitSceneView( width, height, nullptr, 2 );
    
    myViewController = self;
}

- (void)mouseDown:(NSEvent *)theEvent
{
    NSLog(@"mouseDown\n");
    //MouseDown( (int)theEvent.locationInWindow.x, (int)theEvent.locationInWindow.y );
}

- (void)mouseUp:(NSEvent *)theEvent
{
    NSLog(@"mouseUp\n");
    //MouseUp( (int)theEvent.locationInWindow.x, (int)theEvent.locationInWindow.y );
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    NSLog(@"mouseMove\n");
    //MouseMove( (int)theEvent.locationInWindow.x, self.view.bounds.size.height - (int)theEvent.locationInWindow.y );
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    NSLog(@"mouseDrag\n");
    //RotateCamera( theEvent.deltaX, theEvent.deltaY );
    //MouseMove( (int)theEvent.locationInWindow.x, self.view.bounds.size.height - (int)theEvent.locationInWindow.y );
}

- (void)keyDown:(NSEvent *)theEvent
{
    NSLog(@"keyDown\n");
    
    if ([theEvent keyCode] == 0x00) // A
    {
        //MoveRight( 1 );
    }
    else if ([theEvent keyCode] == 0x02) // D
    {
        //MoveRight( -1 );
    }
    else if ([theEvent keyCode] == 0x0D) // W
    {
        //MoveForward( 1 );
    }
    else if ([theEvent keyCode] == 0x01) // S
    {
        //MoveForward( -1 );
    }
    else if ([theEvent keyCode] == 0x0C) // Q
    {
        //MoveUp( 1 );
    }
    else if ([theEvent keyCode] == 0x0E) // E
    {
        //MoveUp( -1 );
    }
}

- (void)keyUp:(NSEvent *)theEvent
{
    NSLog(@"keyUp\n");
    
    if ([theEvent keyCode] == 0x00) // A
    {
        //MoveRight( 0 );
    }
    else if ([theEvent keyCode] == 0x02) // D
    {
        //MoveRight( 0 );
    }
    else if ([theEvent keyCode] == 0x0D) // W
    {
        //MoveForward( 0 );
    }
    else if ([theEvent keyCode] == 0x01) // S
    {
        //MoveForward( 0 );
    }
    else if ([theEvent keyCode] == 0x0C) // Q
    {
        //MoveUp( 0 );
    }
    else if ([theEvent keyCode] == 0x0E) // E
    {
        //MoveUp( 0 );
    }
}

@end
