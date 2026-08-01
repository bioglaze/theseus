#import <Metal/Metal.h>
#import <AppKit/AppKit.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include "window.h"

extern id<CAMetalDrawable> gDrawable;
extern MTLRenderPassDescriptor* renderPassDescriptor;

void HandleEvent( const teWindowEvent& event );
void Init( unsigned width, unsigned height );
void Render();
void Tick();

unsigned width = 800, height = 450;

@interface GameView : MTKView
@end

@implementation GameView
- (id)initWithFrame:(CGRect)inFrame
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    self = [super initWithFrame:inFrame device:device];
    if (self)
    {
        self.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        self.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
        
        Init( width, height );
    }
    return self;
}

- (void)drawRect:(CGRect)rect
{
    renderPassDescriptor = self.currentRenderPassDescriptor;
    gDrawable = self.currentDrawable;
    Tick();
    Render();
}
@end

@interface KeyHandlingWindow: NSWindow
@end

@implementation KeyHandlingWindow

- (void)keyDown:(NSEvent *)theEvent
{
    teWindowEvent event;
    event.type = teWindowEvent::Type::KeyDown;

    if ([theEvent keyCode] == 0x00) // A
    {
        event.keyCode = teWindowEvent::KeyCode::A;
        HandleEvent( event );
    }
    else if ([theEvent keyCode] == 0x02) // D
    {
        event.keyCode = teWindowEvent::KeyCode::D;
        HandleEvent( event );
    }
    else if ([theEvent keyCode] == 0x0D) // W
    {
        event.keyCode = teWindowEvent::KeyCode::W;
        HandleEvent( event );
    }
    else if ([theEvent keyCode] == 0x01) // S
    {
        event.keyCode = teWindowEvent::KeyCode::S;
        HandleEvent( event );
    }
    else if ([theEvent keyCode] == 0x0C) // Q
    {
        event.keyCode = teWindowEvent::KeyCode::Q;
        HandleEvent( event );
    }
    else if ([theEvent keyCode] == 0x0E) // E
    {
        event.keyCode = teWindowEvent::KeyCode::E;
        HandleEvent( event );
    }
}

- (void)keyUp:(NSEvent *)theEvent
{
    teWindowEvent event;
    event.type = teWindowEvent::Type::KeyUp;

    if ([theEvent keyCode] == 0x00) // A
    {
        event.keyCode = teWindowEvent::KeyCode::A;
        HandleEvent( event );
    }
    else if ([theEvent keyCode] == 0x02) // D
    {
        event.keyCode = teWindowEvent::KeyCode::D;
        HandleEvent( event );
    }
    else if ([theEvent keyCode] == 0x0D) // W
    {
        event.keyCode = teWindowEvent::KeyCode::W;
        HandleEvent( event );
    }
    else if ([theEvent keyCode] == 0x01) // S
    {
        event.keyCode = teWindowEvent::KeyCode::S;
        HandleEvent( event );
    }
    else if ([theEvent keyCode] == 0x0C) // Q
    {
        event.keyCode = teWindowEvent::KeyCode::Q;
        HandleEvent( event );
    }
    else if ([theEvent keyCode] == 0x0E) // E
    {
        event.keyCode = teWindowEvent::KeyCode::E;
        HandleEvent( event );
    }
}

- (void)mouseDown:(NSEvent *)theEvent
{
    teWindowEvent event;
    event.type = teWindowEvent::Type::Mouse1Down;
    event.x = (int)theEvent.locationInWindow.x;
    event.y = height - (int)theEvent.locationInWindow.y;
    HandleEvent( event );
}

- (void)mouseUp:(NSEvent *)theEvent
{
    teWindowEvent event;
    event.type = teWindowEvent::Type::Mouse1Up;
    event.x = (int)theEvent.locationInWindow.x;
    event.y = height - (int)theEvent.locationInWindow.y;
    HandleEvent( event );
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    teWindowEvent event;
    event.type = teWindowEvent::Type::MouseMove;
    event.x = (int)theEvent.locationInWindow.x;
    event.y = height - (int)theEvent.locationInWindow.y;
    HandleEvent( event );
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    //teTransformOffsetRotate( m_camera3d.index, Vec3( 0, 1, 0 ), -theEvent.deltaX / 20.0f );
    //teTransformOffsetRotate( m_camera3d.index, Vec3( 1, 0, 0 ), -theEvent.deltaY / 20.0f );
}

@end

int main()
{
    @autoreleasepool
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp activateIgnoringOtherApps:YES];
        
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
        
        GameView* view = [[GameView alloc] initWithFrame:frame];
        window.contentView = view;
        
        [NSApp run];
    }
    
    return 0;
}
