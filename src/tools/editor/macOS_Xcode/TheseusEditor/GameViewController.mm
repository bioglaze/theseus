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
void SelectObject( unsigned x, unsigned y );
NSViewController* myViewController;

Vec3 moveDir;

void GetOpenPath( char* path, const char* extension )
{
    NSMutableArray *types = [NSMutableArray new];
    [types addObject: [NSString stringWithUTF8String: extension]];
    //NSArray<UTType*>* types = [NSArray<UTType*> new];
    
    NSOpenPanel *op = [NSOpenPanel openPanel];
    [op setAllowedFileTypes: types];
    //[op allowedContentTypes:types];
    
    if ([op runModal] == NSModalResponseOK)
    {
        NSURL *nsurl = [[op URLs] objectAtIndex:0];
        NSString *myString = [nsurl path];
        const char* str = [myString UTF8String];
        strcpy( path, str );
    }
    
    path[ 0 ] = 0;
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
    
    if (!io.WantCaptureKeyboard)
    {
        SelectObject( (int)theEvent.locationInWindow.x * uiScale, (int)theEvent.locationInWindow.y * uiScale );
    }
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    //MouseMove( (int)theEvent.locationInWindow.x, self.view.bounds.size.height - (int)theEvent.locationInWindow.y );
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent( (int)theEvent.locationInWindow.x * uiScale, (self.view.bounds.size.height - (int)theEvent.locationInWindow.y) * uiScale );
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent( (int)theEvent.locationInWindow.x * uiScale, (self.view.bounds.size.height - (int)theEvent.locationInWindow.y) * uiScale );
    if (!io.WantCaptureMouse)
        RotateEditorCamera( theEvent.deltaX, theEvent.deltaY );
}

- (void)keyDown:(NSEvent *)theEvent
{
    //NSLog(@"keyDown\n");
    NSLog(@"%d", [theEvent keyCode]);
    
    ImGuiIO& io = ImGui::GetIO();
    
    if ([theEvent keyCode] == 0x00) // A
    {
        if (!io.WantTextInput)
            MoveRight( 1 );
        io.AddInputCharacter( 'a' );
    }
    else if ([theEvent keyCode] == 0x02) // D
    {
        if (!io.WantTextInput)
            MoveRight( -1 );
        io.AddInputCharacter( 'd' );
    }
    else if ([theEvent keyCode] == 0x0D) // W
    {
        if (!io.WantTextInput)
            MoveForward( 1 );
        io.AddInputCharacter( 'w' );
    }
    else if ([theEvent keyCode] == 0x01) // S
    {
        if (!io.WantTextInput)
            MoveForward( -1 );
        io.AddInputCharacter( 's' );
    }
    else if ([theEvent keyCode] == 0x0C) // Q
    {
        if (!io.WantTextInput)
            MoveUp( 1 );
        io.AddInputCharacter( 'q' );
    }
    else if ([theEvent keyCode] == 0x0E) // E
    {
        if (!io.WantTextInput)
            MoveUp( -1 );
        io.AddInputCharacter( 'e' );
    }
    else if ([theEvent keyCode] == 18)
        io.AddInputCharacter( '1' );
    else if ([theEvent keyCode] == 19)
        io.AddInputCharacter( '2' );
    else if ([theEvent keyCode] == 20)
        io.AddInputCharacter( '3' );
    else if ([theEvent keyCode] == 21)
        io.AddInputCharacter( '4' );
    else if ([theEvent keyCode] == 23)
        io.AddInputCharacter( '5' );
    else if ([theEvent keyCode] == 22)
        io.AddInputCharacter( '6' );
    else if ([theEvent keyCode] == 26)
        io.AddInputCharacter( '7' );
    else if ([theEvent keyCode] == 28)
        io.AddInputCharacter( '8' );
    else if ([theEvent keyCode] == 25)
        io.AddInputCharacter( '9' );
    else if ([theEvent keyCode] == 29)
        io.AddInputCharacter( '0' );
    else if ([theEvent keyCode] == 49)
        io.AddInputCharacter( ' ' );
    else if ([theEvent keyCode] == 11)
        io.AddInputCharacter( 'b' );
    else if ([theEvent keyCode] == 8)
        io.AddInputCharacter( 'c' );
    else if ([theEvent keyCode] == 3)
        io.AddInputCharacter( 'f' );
    else if ([theEvent keyCode] == 5)
        io.AddInputCharacter( 'g' );
    else if ([theEvent keyCode] == 4)
        io.AddInputCharacter( 'h' );
    else if ([theEvent keyCode] == 34)
        io.AddInputCharacter( 'i' );
    else if ([theEvent keyCode] == 38)
        io.AddInputCharacter( 'j' );
    else if ([theEvent keyCode] == 40)
        io.AddInputCharacter( 'k' );
    else if ([theEvent keyCode] == 37)
        io.AddInputCharacter( 'l' );
    else if ([theEvent keyCode] == 46)
        io.AddInputCharacter( 'm' );
    else if ([theEvent keyCode] == 45)
        io.AddInputCharacter( 'n' );
    else if ([theEvent keyCode] == 31)
        io.AddInputCharacter( 'o' );
    else if ([theEvent keyCode] == 35)
        io.AddInputCharacter( 'p' );
    else if ([theEvent keyCode] == 15)
        io.AddInputCharacter( 'r' );
    else if ([theEvent keyCode] == 17)
        io.AddInputCharacter( 't' );
    else if ([theEvent keyCode] == 32)
        io.AddInputCharacter( 'u' );
    else if ([theEvent keyCode] == 9)
        io.AddInputCharacter( 'v' );
    else if ([theEvent keyCode] == 7)
        io.AddInputCharacter( 'x' );
    else if ([theEvent keyCode] == 16)
        io.AddInputCharacter( 'y' );
    else if ([theEvent keyCode] == 6)
        io.AddInputCharacter( 'z' );
    else if ([theEvent keyCode] == 47)
        io.AddInputCharacter( '.' );
    else if ([theEvent keyCode] == 43)
        io.AddInputCharacter( ',' );
    else if ([theEvent keyCode] == 44)
        io.AddInputCharacter( '-' );
    else if ([theEvent keyCode] == 27)
        io.AddInputCharacter( '+' );
    else if ([theEvent keyCode] == 50)
        io.AddInputCharacter( '<' );

    else if ([theEvent keyCode] == 123)
        io.AddKeyEvent( ImGuiKey_LeftArrow, true );
    else if ([theEvent keyCode] == 124)
        io.AddKeyEvent( ImGuiKey_RightArrow, true );
    else if ([theEvent keyCode] == 126)
        io.AddKeyEvent( ImGuiKey_UpArrow, true );
    else if ([theEvent keyCode] == 125)
        io.AddKeyEvent( ImGuiKey_DownArrow, true );
    else if ([theEvent keyCode] == 51)
        io.AddKeyEvent( ImGuiKey_Backspace, true );

}

- (void)keyUp:(NSEvent *)theEvent
{
    NSLog(@"keyUp\n");
    ImGuiIO& io = ImGui::GetIO();
    
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
    else if ([theEvent keyCode] == 123)
        io.AddKeyEvent( ImGuiKey_LeftArrow, false );
    else if ([theEvent keyCode] == 124)
        io.AddKeyEvent( ImGuiKey_RightArrow, false );
    else if ([theEvent keyCode] == 126)
        io.AddKeyEvent( ImGuiKey_UpArrow, false );
    else if ([theEvent keyCode] == 125)
        io.AddKeyEvent( ImGuiKey_DownArrow, false );
    else if ([theEvent keyCode] == 51)
        io.AddKeyEvent( ImGuiKey_Backspace, false );
}

@end
