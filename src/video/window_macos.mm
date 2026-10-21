#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#include "window.h"

bool IncEventIndex();

constexpr int EventArraySize = 100;
CAMetalLayer* gMetalLayer;
id<MTLDevice> gDevice;

struct WindowImpl
{
    teWindowEvent events[ EventArraySize ];
    teWindowEvent::KeyCode keyMap[ 256 ] = {};
    int eventIndex = -1;
    unsigned width = 0;
    unsigned height = 0;
    float scale = 1;
};

WindowImpl win;

@protocol MetalViewDelegate <NSObject>
@end

@interface MetalView: NSView<CALayerDelegate>

@property (nonatomic, nonnull, readonly) CAMetalLayer* metalLayer;
@property (nonatomic, nullable) id<MetalViewDelegate> delegate;

@end

@implementation MetalView

- (CALayer *)makeBackingLayer
{
    gMetalLayer = [CAMetalLayer layer];
    gMetalLayer.device = gDevice;
    gMetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    gMetalLayer.contentsScale = win.scale;

    return gMetalLayer;
}

@end

@interface MetalViewController : NSViewController <MetalViewDelegate>

@end

@implementation MetalViewController
{
}

@end

@interface KeyHandlingWindow: NSWindow
@end

@implementation KeyHandlingWindow

- (void)keyDown:(NSEvent *)theEvent
{
    int key = [theEvent keyCode];
    //printf("keyDown %d\n", key);

    bool isCmdDown = [theEvent modifierFlags] & NSEventModifierFlagCommand;
    bool isShiftDown = [theEvent modifierFlags] & NSEventModifierFlagShift;
    bool isCtrlDown = [theEvent modifierFlags] & NSEventModifierFlagControl;

    unsigned keyModifiers = 0;
    if (isCmdDown) keyModifiers |= (unsigned)teWindowEvent::KeyModifier::Control;
    if (isShiftDown) keyModifiers |= (unsigned)teWindowEvent::KeyModifier::Shift;
    //if (isCtrlDown) keyModifiers |= (unsigned)teWindowEvent::KeyModifier::Control;

    if ([theEvent keyCode] == 0x00 && IncEventIndex()) // A
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::A;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 0x02 && IncEventIndex()) // D
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::D;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 0x0D && IncEventIndex()) // W
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::W;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 0x01 && IncEventIndex()) // S
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::S;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 0x0C && IncEventIndex()) // Q
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Q;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 0x0E && IncEventIndex()) // E
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::E;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 123 && IncEventIndex()) // Left
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Left;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;        
    }
    else if ([theEvent keyCode] == 124 && IncEventIndex()) // Right
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Right;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;        
    }
    else if ([theEvent keyCode] == 126 && IncEventIndex()) // Up
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Up;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;        
    }
    else if ([theEvent keyCode] == 125 && IncEventIndex()) // Down
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Down;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 51 && IncEventIndex()) // Backspace
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Backspace;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 27 && IncEventIndex()) // +
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Plus;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
        else if ([theEvent keyCode] == 44 && IncEventIndex()) // -
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Minus;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
}

- (void)keyUp:(NSEvent *)theEvent
{
    int key = [theEvent keyCode];
    //printf("keyDown %d\n", key);

    bool isCmdDown = [theEvent modifierFlags] & NSEventModifierFlagCommand;
    bool isShiftDown = [theEvent modifierFlags] & NSEventModifierFlagShift;
    bool isCtrlDown = [theEvent modifierFlags] & NSEventModifierFlagControl;

    unsigned keyModifiers = 0;
    if (isCmdDown) keyModifiers |= (unsigned)teWindowEvent::KeyModifier::Control;
    if (isShiftDown) keyModifiers |= (unsigned)teWindowEvent::KeyModifier::Shift;
    //if (isCtrlDown) keyModifiers |= (unsigned)teWindowEvent::KeyModifier::Control;

    if ([theEvent keyCode] == 0x00 && IncEventIndex()) // A
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::A;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 0x02 && IncEventIndex()) // D
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::D;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 0x0D && IncEventIndex()) // W
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::W;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 0x01 && IncEventIndex()) // S
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::S;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 0x0C && IncEventIndex()) // Q
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Q;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 0x0E && IncEventIndex()) // E
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::E;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 123 && IncEventIndex()) // Left
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Left;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 124 && IncEventIndex()) // Right
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Right;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 126 && IncEventIndex()) // Up
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Up;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 125 && IncEventIndex()) // Down
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Down;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 51 && IncEventIndex()) // Backspace
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Backspace;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 27 && IncEventIndex()) // +
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Plus;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 44 && IncEventIndex()) // -
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Minus;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
}

- (void)mouseDown:(NSEvent *)theEvent
{
    if (IncEventIndex())
    {
        float x = theEvent.locationInWindow.x * win.scale;
        float y = theEvent.locationInWindow.y * win.scale;

        win.events[ win.eventIndex ].type = teWindowEvent::Type::Mouse1Down;
        win.events[ win.eventIndex ].x = x >= 0 ? x : 0;
        win.events[ win.eventIndex ].y = win.height - y >= 0 ? y : 0;
    }
}

- (void)mouseUp:(NSEvent *)theEvent
{
    if (IncEventIndex())
    {
        float x = theEvent.locationInWindow.x * win.scale;
        float y = theEvent.locationInWindow.y * win.scale;

        win.events[ win.eventIndex ].type = teWindowEvent::Type::Mouse1Up;
        win.events[ win.eventIndex ].x = x >= 0 ? x : 0;
        win.events[ win.eventIndex ].y = win.height - y >= 0 ? y : 0;
    }
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    if (IncEventIndex())
    {
        float x = theEvent.locationInWindow.x * win.scale;
        float y = theEvent.locationInWindow.y * win.scale;

        win.events[ win.eventIndex ].type = teWindowEvent::Type::MouseMove;
        win.events[ win.eventIndex ].x = x >= 0 ? x : 0;
        win.events[ win.eventIndex ].y = win.height - y >= 0 ? y : 0;
    }
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    if (IncEventIndex())
    {
        float x = theEvent.locationInWindow.x * win.scale;
        float y = theEvent.locationInWindow.y * win.scale;

        win.events[ win.eventIndex ].type = teWindowEvent::Type::MouseMove;
        win.events[ win.eventIndex ].x = x >= 0 ? x : 0;
        win.events[ win.eventIndex ].y = win.height - y >= 0 ? y : 0;
    }
}

@end

void* teCreateWindow( unsigned width, unsigned height, const char* title )
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
    
    NSRect rect = NSMakeRect( 0, 0, width, height );
    NSRect frame = NSMakeRect( 0, 0, width, height );
    NSWindow* window = [[KeyHandlingWindow alloc]
                        initWithContentRect:rect
                        styleMask:NSWindowStyleMaskTitled
                        backing:NSBackingStoreBuffered
                        defer:NO];
    [window cascadeTopLeftFromPoint:NSMakePoint( 20, 20 )];
    //window.styleMask |= NSWindowStyleMaskResizable;
    window.styleMask |= NSWindowStyleMaskMiniaturizable;
    window.styleMask |= NSWindowStyleMaskClosable;
    window.title = [[NSProcessInfo processInfo] processName];
    [window makeKeyAndOrderFront:nil];
    [window setAcceptsMouseMovedEvents:YES];

    win.scale = [window backingScaleFactor];
    printf("scale: %f\n", win.scale );

    unsigned frameWidth = (unsigned)window.frame.size.width;
    unsigned frameHeight = (unsigned)window.frame.size.height;
    printf( "width: %u, height: %u, framew %u, frameh %u\n", width, height, frameWidth, frameHeight);

    win.width = frameWidth * win.scale;
    win.height = frameHeight * win.scale;

    gDevice = MTLCreateSystemDefaultDevice();

    MetalView* view = [[MetalView alloc] initWithFrame:frame];
    view.wantsLayer = YES;

    MetalViewController* viewController = [[MetalViewController alloc] init];
    view.delegate = viewController;

    window.contentView = view;

    return nullptr;
}

void teWindowGetSize( unsigned& outWidth, unsigned& outHeight )
{
    outWidth = win.width;
    outHeight = win.height;
}

bool IncEventIndex()
{
    if (win.eventIndex < EventArraySize - 1)
    {
        ++win.eventIndex;
        return true;
    }

    return false;
}

void tePushWindowEvents()
{
    bool isDone = false;

    while (!isDone)
    {
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                        untilDate:[NSDate distantPast]
                                           inMode:NSDefaultRunLoopMode
                                          dequeue:YES];
        if (event)
        {
            [NSApp sendEvent:event];
        }
        else
        {
            isDone = true;
        }
    }
}

teWindowEvent* teGetWindowEvents()
{
    return win.events;
}

unsigned teGetWindowEventCount()
{
    return win.eventIndex + 1;
}

void teClearWindowEvents()
{
    win.eventIndex = -1;

    for (unsigned i = 0; i < EventArraySize; ++i)
    {
        win.events[ i ].type = teWindowEvent::Type::Empty;
        win.events[ i ].x = 0;
        win.events[ i ].y = 0;
        win.events[ i ].keyModifiers = 0;
    }
}
