#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <Metal/Metal.h>
#include "window.h"

bool IncEventIndex();

constexpr int EventArraySize = 100;
extern id<MTLCommandBuffer> gCommandBuffer;
CAMetalLayer* gMetalLayer;
id<MTLDevice> gDevice;

struct WindowImpl
{
    teWindowEvent events[ EventArraySize ];
    int eventIndex = -1;
    unsigned width = 0;
    unsigned height = 0;
    float scale = 1;
    dispatch_semaphore_t inFlightSemaphore;
};

WindowImpl win;

void GetOpenPath( char* path, const char* extension )
{
@autoreleasepool
    {

    NSMutableArray<UTType*>* types = [NSMutableArray<UTType*> new];
    UTType* type = [UTType typeWithFilenameExtension: [NSString stringWithUTF8String: extension]];

    if (type != nil)
    {
        [types addObject: type];
    }

    NSOpenPanel *op = [NSOpenPanel openPanel];
    [op setAllowedContentTypes: types];
    
    if ([op runModal] == NSModalResponseOK)
    {
        NSURL *nsurl = [[op URLs] objectAtIndex:0];
        NSString *myString = [nsurl path];
        const char* str = [myString UTF8String];
        
        if (strlen( str ) < 280)
        {
            strcpy( path, str );
        }
        else
        {
            path[ 0 ] = 0;
        }
    }
    else
    {
        path[ 0 ] = 0;
    }

    }
}

void GetSavePath( char* path, const char* extension )
{
    NSSavePanel *op = [NSSavePanel savePanel];
    
    if ([op runModal] == NSModalResponseOK)
    {
        NSURL *nsurl = [op URL];
        NSString *myString = [nsurl path];
        const char* str = [myString UTF8String];

        if (strlen( str ) < 280)
        {
            strcpy( path, str );
            path[ strlen( str ) ] = 0;
        }
        else
        {
            path[ 0 ] = 0;
        }
    }
    else
    {
        path[ 0 ] = 0;
    }
}

@interface MetalView: NSView<CALayerDelegate>
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

@interface KeyHandlingWindow: NSWindow
@end

@implementation KeyHandlingWindow

- (void)keyDown:(NSEvent *)theEvent
{
    int key = [theEvent keyCode];
    printf("keyDown %d\n", key);

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
    else if ([theEvent keyCode] == 11 && IncEventIndex()) // B
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::B;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 8 && IncEventIndex()) // C
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::C;
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
    else if ([theEvent keyCode] == 3 && IncEventIndex()) // F
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::F;
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
    else if ([theEvent keyCode] == 18 && IncEventIndex()) // 1
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N1;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 19 && IncEventIndex()) // 2
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N2;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 20 && IncEventIndex()) // 3
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N3;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 21 && IncEventIndex()) // 4
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N4;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 23 && IncEventIndex()) // 5
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N5;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 22 && IncEventIndex()) // 6
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N6;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 26 && IncEventIndex()) // 7
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N7;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 28 && IncEventIndex()) // 8
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N8;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 25 && IncEventIndex()) // 9
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N9;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 29 && IncEventIndex()) // 0
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N0;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 36 && IncEventIndex()) // Enter
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Enter;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 49 && IncEventIndex()) // Space
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Space;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 48 && IncEventIndex()) // Tab
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Tab;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 53 && IncEventIndex()) // Escape
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Escape;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }

}

- (void)keyUp:(NSEvent *)theEvent
{
    int key = [theEvent keyCode];
    //printf("keyUp %d\n", key);

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
    else if ([theEvent keyCode] == 11 && IncEventIndex()) // B
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::B;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 8 && IncEventIndex()) // C
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::C;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 0x02 && IncEventIndex()) // D
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::D;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 3 && IncEventIndex()) // F
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::F;
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
    else if ([theEvent keyCode] == 18 && IncEventIndex()) // 1
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N1;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 19 && IncEventIndex()) // 2
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N2;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 20 && IncEventIndex()) // 3
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N3;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 21 && IncEventIndex()) // 4
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N4;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 23 && IncEventIndex()) // 5
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N5;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 22 && IncEventIndex()) // 6
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N6;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 26 && IncEventIndex()) // 7
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N7;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 28 && IncEventIndex()) // 8
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N8;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 25 && IncEventIndex()) // 9
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N9;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 29 && IncEventIndex()) // 0
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::N0;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 36 && IncEventIndex()) // Enter
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Enter;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 49 && IncEventIndex()) // Space
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Space;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 48 && IncEventIndex()) // Tab
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Tab;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }
    else if ([theEvent keyCode] == 53 && IncEventIndex()) // Escape
    {
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = teWindowEvent::KeyCode::Escape;
        win.events[ win.eventIndex ].keyModifiers = keyModifiers;
    }

}

- (void)mouseDown:(NSEvent *)theEvent
{
    if (IncEventIndex())
    {
        float x = theEvent.locationInWindow.x * win.scale;
        float y = theEvent.locationInWindow.y * win.scale;
        y = win.height - y;

        win.events[ win.eventIndex ].type = teWindowEvent::Type::Mouse1Down;
        win.events[ win.eventIndex ].x = x >= 0 ? x : 0;
        win.events[ win.eventIndex ].y = y >= 0 ? y : 0;
    }
}

- (void)mouseUp:(NSEvent *)theEvent
{
    if (IncEventIndex())
    {
        float x = theEvent.locationInWindow.x * win.scale;
        float y = theEvent.locationInWindow.y * win.scale;
        y = win.height - y;

        win.events[ win.eventIndex ].type = teWindowEvent::Type::Mouse1Up;
        win.events[ win.eventIndex ].x = x >= 0 ? x : 0;
        win.events[ win.eventIndex ].y = y >= 0 ? y : 0;
    }
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    if (IncEventIndex())
    {
        float x = theEvent.locationInWindow.x * win.scale;
        float y = theEvent.locationInWindow.y * win.scale;
        y = win.height - y;

        win.events[ win.eventIndex ].type = teWindowEvent::Type::MouseMove;
        win.events[ win.eventIndex ].x = x >= 0 ? x : 0;
        win.events[ win.eventIndex ].y = y >= 0 ? y : 0;
    }
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    if (IncEventIndex())
    {
        float x = theEvent.locationInWindow.x * win.scale;
        float y = theEvent.locationInWindow.y * win.scale;
        y = win.height - y;

        win.events[ win.eventIndex ].type = teWindowEvent::Type::MouseMove;
        win.events[ win.eventIndex ].x = x >= 0 ? x : 0;
        win.events[ win.eventIndex ].y = y >= 0 ? y : 0;
    }
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    printf("scrollwheel\n");
    if (IncEventIndex())
    {
        float x = theEvent.locationInWindow.x * win.scale;
        float y = theEvent.locationInWindow.y * win.scale;
        y = win.height - y;

        win.events[ win.eventIndex ].type = teWindowEvent::Type::MouseWheel;
        win.events[ win.eventIndex ].x = x >= 0 ? x : 0;
        win.events[ win.eventIndex ].y = y >= 0 ? y : 0;
        win.events[ win.eventIndex ].wheelDelta = theEvent.deltaY;
        bool invert = [theEvent isDirectionInvertedFromDevice];
        //printf("delta %f, invert: %d\n", theEvent.deltaY, invert);
    }
}

- (void)rightMouseDragged:(NSEvent *)theEvent
{
    printf("rightMouseDragged\n");
}

@end

void* teCreateWindow( unsigned width, unsigned height, const char* title )
{
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    //[NSApp activateIgnoringOtherApps:YES];

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
    //[window cascadeTopLeftFromPoint:NSMakePoint( 20, 20 )];
    //window.styleMask |= NSWindowStyleMaskResizable;
    window.styleMask |= NSWindowStyleMaskMiniaturizable;
    window.styleMask |= NSWindowStyleMaskClosable;
    window.title = [NSString stringWithUTF8String:title];
    [window setAcceptsMouseMovedEvents:YES];

    win.scale = [window backingScaleFactor];

    gDevice = MTLCreateSystemDefaultDevice();

    MetalView* view = [[MetalView alloc] initWithFrame:frame];
    view.wantsLayer = YES;

    window.contentView = view;

    [window center];
    [window makeKeyAndOrderFront:nil];

    unsigned frameWidth = (unsigned)window.contentView.frame.size.width;
    unsigned frameHeight = (unsigned)window.contentView.frame.size.height;
    //printf( "width: %u, height: %u, framew %u, frameh %u\n", width, height, frameWidth, frameHeight);

    win.width = frameWidth * win.scale;
    win.height = frameHeight * win.scale;
    
    [NSApp finishLaunching];

    unsigned maxFramesInFlight = 3;
    win.inFlightSemaphore = dispatch_semaphore_create( maxFramesInFlight );

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
    @autoreleasepool
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
        win.events[ i ] = teWindowEvent{};
    }
}

void WaitForPreviousFrame()
{
    dispatch_semaphore_wait( win.inFlightSemaphore, DISPATCH_TIME_FOREVER );
}

void AddCompletedHandler()
{
    __block dispatch_semaphore_t block_semaphore = win.inFlightSemaphore;
    [gCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer)
     {
         dispatch_semaphore_signal( block_semaphore );
     }];
}
