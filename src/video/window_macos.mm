#import <Cocoa/Cocoa.h>
#include "window.h"

constexpr int EventStackSize = 100;

struct WindowImpl
{
    teWindowEvent events[ EventStackSize ];
    teWindowEvent::KeyCode keyMap[ 256 ] = {};
    int eventIndex = -1;
    unsigned windowWidthWithoutTitleBar = 0;
    unsigned windowHeightWithoutTitleBar = 0;
    unsigned width = 0;
    unsigned height = 0;
};

WindowImpl win;

void* teCreateWindow( unsigned width, unsigned height, const char* title )
{
    NSRect frame = NSMakeRect( 0, 0, width, height );
    NSWindow* window = [[[NSWindow alloc] initWithContentRect:frame
                    styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                    defer:NO] autorelease];
    [window setBackgroundColor:[NSColor blueColor]];
    [window makeKeyAndOrderFront:NSApp];

    win.width = width;
    win.height = height;
    
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

const teWindowEvent& tePopWindowEvent()
{
    if (win.eventIndex == -1)
    {
        win.events[ 0 ].type = teWindowEvent::Type::Empty;
        return win.events[ 0 ];
    }

    --win.eventIndex;
    return win.events[ win.eventIndex + 1 ];
}
