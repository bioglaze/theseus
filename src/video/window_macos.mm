#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#include "window.h"

constexpr int EventStackSize = 100;
extern MTLRenderPassDescriptor* renderPassDescriptor;

struct WindowImpl
{
    teWindowEvent events[ EventStackSize ];
    teWindowEvent::KeyCode keyMap[ 256 ] = {};
    int eventIndex = -1;
    unsigned width = 0;
    unsigned height = 0;
};

WindowImpl win;

@protocol MetalViewDelegate <NSObject>

- (void)drawableResize:(CGSize)size;

- (void)renderToMetalLayer:(nonnull CAMetalLayer *)metalLayer;

@end

@interface MetalView: NSView<CALayerDelegate>

@property (nonatomic, nonnull, readonly) CAMetalLayer* metalLayer;
@property (nonatomic, nullable) id<MetalViewDelegate> delegate;

@end

@implementation MetalView

@end

@interface KeyHandlingWindow: NSWindow
@end

@implementation KeyHandlingWindow

- (void)keyDown:(NSEvent *)theEvent
{
    printf("keyDown\n");
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

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();

    MetalView* view = [[MetalView alloc] initWithFrame:frame];
    view.metalLayer.device = device;
    view.metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    
    window.contentView = view;
    
    [NSApp run];

    return nullptr;
}

void teWindowGetSize( unsigned& outWidth, unsigned& outHeight )
{
    outWidth = win.width;
    outHeight = win.height;
}

void tePushWindowEvents()
{

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

    for (unsigned i = 0; i < EventStackSize; ++i)
    {
        win.events[ i ].type = teWindowEvent::Type::Empty;
    }
}
