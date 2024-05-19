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

const int uiScale = 2;

void InitSceneView( unsigned width, unsigned height, void* windowHandle, int uiScale );
void RotateEditorCamera( float x, float y );
NSViewController* myViewController;

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
    InitSceneView( width, height, nullptr, uiScale );
    
    myViewController = self;
}

- (void)mouseDown:(NSEvent *)theEvent
{
    //MouseDown( (int)theEvent.locationInWindow.x, (int)theEvent.locationInWindow.y );
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent( 0, true );
}

- (void)mouseUp:(NSEvent *)theEvent
{
    //MouseUp( (int)theEvent.locationInWindow.x, (int)theEvent.locationInWindow.y );
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent( 0, false );
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    //MouseMove( (int)theEvent.locationInWindow.x, self.view.bounds.size.height - (int)theEvent.locationInWindow.y );
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent( (int)theEvent.locationInWindow.x * uiScale, (self.view.bounds.size.height - (int)theEvent.locationInWindow.y) * uiScale );
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    RotateEditorCamera( theEvent.deltaX, theEvent.deltaY );
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent( (int)theEvent.locationInWindow.x * uiScale, (self.view.bounds.size.height - (int)theEvent.locationInWindow.y) * uiScale );
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
