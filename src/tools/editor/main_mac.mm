#import <Metal/Metal.h>
#import <AppKit/AppKit.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "imgui.h"
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

void InitSceneView( unsigned width, unsigned height, void* windowHandle, int uiScale );
void RenderSceneView( float gridStep );
unsigned SceneViewGetCameraIndex();
void SelectObject( unsigned x, unsigned y );
void SelectGizmo( unsigned x, unsigned y );
void DeleteSelectedObject();
void SceneMouseMove( float x, float y, float dx, float dy, bool isLeftMouseDown );
void SceneMoveSelection( Vec3 amount );

extern id<CAMetalDrawable> gDrawable;
extern MTLRenderPassDescriptor* renderPassDescriptor;
extern id<MTLCommandBuffer> gCommandBuffer;

unsigned width = 800, height = 450;
const int uiScale = 1;

struct InputParams
{
    int x = 0;
    int y = 0;
    float deltaX = 0;
    float deltaY = 0;
    int lastMouseX = 0;
    int lastMouseY = 0;

    Vec3 moveDir;
} inputParams;

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
        InitSceneView( width, height, nullptr, 2 );
        teFinalizeMeshBuffers();
    }
    return self;
}

- (void)drawRect:(CGRect)rect
{
    Vec3 moveDir = inputParams.moveDir;

    float dt = 1;

    teTransformMoveForward( SceneViewGetCameraIndex(), moveDir.z * (float)dt * 0.5f, false, false, false );
    teTransformMoveRight( SceneViewGetCameraIndex(), moveDir.x * (float)dt * 0.5f );
    teTransformMoveUp( SceneViewGetCameraIndex(), moveDir.y * (float)dt * 0.5f );

    renderPassDescriptor = self.currentRenderPassDescriptor;

    gDrawable = self.currentDrawable;
    RenderSceneView( 1 );
}
@end

@interface KeyHandlingWindow: NSWindow
@end

@implementation KeyHandlingWindow

- (void)keyDown:(NSEvent *)theEvent
{
    bool isCmdDown = [theEvent modifierFlags] & NSEventModifierFlagCommand;
    bool isShiftDown = [theEvent modifierFlags] & NSEventModifierFlagShift;
    bool isCtrlDown = [theEvent modifierFlags] & NSEventModifierFlagControl;
    
    ImGuiIO& io = ImGui::GetIO();
    io.KeyShift = isShiftDown;
    io.KeySuper = isCmdDown;

    if ([theEvent keyCode] == 0x00) // A
    {
        if (!io.WantCaptureKeyboard)
            inputParams.moveDir.x = -0.5f;
    }
    else if ([theEvent keyCode] == 0x02) // D
    {
        if (!io.WantCaptureKeyboard)
            inputParams.moveDir.x = 0.5f;
    }
    else if ([theEvent keyCode] == 0x0D) // W
    {
        if (!io.WantCaptureKeyboard)
            inputParams.moveDir.z = 0.5f;
    }
    else if ([theEvent keyCode] == 0x01) // S
    {
        if (!io.WantCaptureKeyboard)
            inputParams.moveDir.z = -0.5f;
    }
    else if ([theEvent keyCode] == 0x0C) // Q
    {
        if (!io.WantCaptureKeyboard)
            inputParams.moveDir.y = -0.5f;
    }
    else if ([theEvent keyCode] == 0x0E) // E
    {
        if (!io.WantCaptureKeyboard)
            inputParams.moveDir.y = 0.5f;
    }
}

- (void)keyUp:(NSEvent *)theEvent
{
    ImGuiIO& io = ImGui::GetIO();

    if ([theEvent keyCode] == 0x00) // A
    {
        inputParams.moveDir.x = 0;
    }
    else if ([theEvent keyCode] == 0x02) // D
    {
        inputParams.moveDir.x = 0;
    }
    else if ([theEvent keyCode] == 0x0D) // W
    {
        inputParams.moveDir.z = 0;
    }
    else if ([theEvent keyCode] == 0x01) // S
    {
        inputParams.moveDir.z = 0;
    }
    else if ([theEvent keyCode] == 0x0C) // Q
    {
        inputParams.moveDir.y = 0;
    }
    else if ([theEvent keyCode] == 0x0E) // E
    {
        inputParams.moveDir.y = 0;
    }
}

- (void)mouseDown:(NSEvent *)theEvent
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent( 0, true );
}

- (void)mouseUp:(NSEvent *)theEvent
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent( 0, false );

    inputParams.x = theEvent.locationInWindow.x >= 0 ? theEvent.locationInWindow.x : 0;
    inputParams.y = theEvent.locationInWindow.y >= 0 ? theEvent.locationInWindow.y : 0;
    
    if (!io.WantCaptureKeyboard)
    {
        printf("mouseUp x: %d, y: %d\n", (int)theEvent.locationInWindow.x * uiScale, (int)theEvent.locationInWindow.y * uiScale);
        SelectObject( (int)inputParams.x * uiScale, (int)(height - inputParams.y) * uiScale );
    }
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent( (int)theEvent.locationInWindow.x * uiScale * 2, (height - (int)theEvent.locationInWindow.y) * uiScale * 2);

    inputParams.x = theEvent.locationInWindow.x >= 0 ? theEvent.locationInWindow.x : 0;
    inputParams.y = theEvent.locationInWindow.y >= 0 ? theEvent.locationInWindow.y : 0;

    SceneMouseMove( (float)inputParams.x, (float)(height - inputParams.y), inputParams.deltaX, inputParams.deltaY, false/*inputParams.isLeftMouseDown*/ );
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    ImGuiIO& io = ImGui::GetIO();

    inputParams.x = theEvent.locationInWindow.x;
    inputParams.y = theEvent.locationInWindow.y;
    inputParams.deltaX = theEvent.deltaX;//float( inputParams.x - inputParams.lastMouseX );
    inputParams.deltaY = theEvent.deltaY;//float( inputParams.y - inputParams.lastMouseY );
    inputParams.lastMouseX = inputParams.x;
    inputParams.lastMouseY = inputParams.y;

    io.AddMousePosEvent( (int)theEvent.locationInWindow.x * uiScale * 2, (height - (int)theEvent.locationInWindow.y) * uiScale * 2 );

    if (/*inputParams.isRightMouseDown &&*/ !io.WantCaptureMouse)
    {
        float dt = 2;
        teTransformOffsetRotate( SceneViewGetCameraIndex(), Vec3( 0, 1, 0 ), -inputParams.deltaX / 100.0f * (float)dt );
        teTransformOffsetRotate( SceneViewGetCameraIndex(), Vec3( 1, 0, 0 ), -inputParams.deltaY / 100.0f * (float)dt );
    }
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
        [window setAcceptsMouseMovedEvents:YES];

        HelloMetalView* view = [[HelloMetalView alloc] initWithFrame:frame];
        window.contentView = view;
        
        [NSApp run];
        NSLog(@"run()");
    }
    
    return 0;
}
