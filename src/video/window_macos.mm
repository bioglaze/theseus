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

void teWindowGetSize( unsigned& outWidth, unsigned& outHeight )
{
    outWidth = win.width;
    outHeight = win.height;
}

void tePushWindowEvents()
{

}

