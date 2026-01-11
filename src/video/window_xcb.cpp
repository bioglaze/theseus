#include "window.h"
#include "te_stdlib.h"
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_ewmh.h>
#include <X11/keysym.h>
#include <X11/Xlib-xcb.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <dirent.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

xcb_connection_t* connection;
xcb_window_t window;

constexpr int EventStackSize = 100;

struct GamePad
{
    bool isActive = false;
    int fd;
    int buttonA;
    int buttonB;
    int buttonX;
    int buttonY;
    int buttonBack;
    int buttonStart;
    int buttonLeftShoulder;
    int buttonRightShoulder;
    int dpadXaxis;
    int dpadYaxis;
    int leftThumbX;
    int leftThumbY;
    int rightThumbX;
    int rightThumbY;
    short deadZone;

    float lastLeftThumbX = 0;
    float lastLeftThumbY = 0;
    float lastRightThumbX = 0;
    float lastRightThumbY = 0;
};

struct WindowImpl
{
    teWindowEvent          events[ EventStackSize ];
    teWindowEvent::KeyCode keyMap[ 256 ] = {};
    int                    eventIndex = -1;
    unsigned               width = 0;
    unsigned               height = 0;

    // FIXME: this is a hack to get mouse coordinate into mouse move event.
    //       A proper fix would be to read mouse coordinate when mouse move event is detected.
    unsigned              lastMouseX = 0;
    unsigned              lastMouseY = 0;

    GamePad               gamePad;
    bool                  pointerOutsideWindow = false;
    Display*              display = nullptr;
    xcb_key_symbols_t*    key_symbols = nullptr;
    xcb_atom_t            wm_protocols;
    xcb_atom_t            wm_delete_window;

    xcb_ewmh_connection_t EWMH;
    xcb_intern_atom_cookie_t* EWMHCookie = nullptr;
};

WindowImpl win;

float ProcessGamePadStickValue( short value, short deadZoneThreshold )
{
    float result = 0;
        
    if (value < -deadZoneThreshold)
    {
        result = (value + deadZoneThreshold) / (32768.0f - deadZoneThreshold);
    }
    else if (value > deadZoneThreshold)
    {
        result = (value - deadZoneThreshold) / (32767.0f - deadZoneThreshold);
    }

    return result;
}

double GetMilliseconds()
{
    timespec spec;
    clock_gettime( CLOCK_MONOTONIC, &spec );
    return spec.tv_nsec / 1000000;
}

void IncEventIndex()
{
    if (win.eventIndex < EventStackSize - 1)
    {
        ++win.eventIndex;
    }
}

static void InitKeyMap()
{
    for (unsigned keyIndex = 0; keyIndex < 256; ++keyIndex)
    {
        win.keyMap[ keyIndex ] = teWindowEvent::KeyCode::Invalid;
    }

    win.keyMap[ 28 ] = teWindowEvent::KeyCode::Enter;
    win.keyMap[ 105 ] = teWindowEvent::KeyCode::Left;
    win.keyMap[ 103 ] = teWindowEvent::KeyCode::Up;
    win.keyMap[ 106 ] = teWindowEvent::KeyCode::Right;
    win.keyMap[ 108 ] = teWindowEvent::KeyCode::Down;
    win.keyMap[  1 ] = teWindowEvent::KeyCode::Escape;
    win.keyMap[ 57 ] = teWindowEvent::KeyCode::Space;
    win.keyMap[ 14 ] = teWindowEvent::KeyCode::Backspace;
    win.keyMap[ 111 ] = teWindowEvent::KeyCode::Delete;
    win.keyMap[ 52 ] = teWindowEvent::KeyCode::Dot;
    win.keyMap[ 102 ] = teWindowEvent::KeyCode::Home;
    win.keyMap[ 107 ] = teWindowEvent::KeyCode::End;
    win.keyMap[ 12 ] = teWindowEvent::KeyCode::Plus;
    win.keyMap[ 53 ] = teWindowEvent::KeyCode::Minus;
    win.keyMap[ 86 ] = teWindowEvent::KeyCode::Less;
    win.keyMap[ 78 ] = teWindowEvent::KeyCode::NumpadPlus;
    win.keyMap[ 74 ] = teWindowEvent::KeyCode::NumpadMinus;
    win.keyMap[ 15 ] = teWindowEvent::KeyCode::Tab;

    win.keyMap[ 39 ] = teWindowEvent::KeyCode::OE;
    win.keyMap[ 40 ] = teWindowEvent::KeyCode::AE;
    win.keyMap[ 30 ] = teWindowEvent::KeyCode::A;
    win.keyMap[ 48 ] = teWindowEvent::KeyCode::B;
    win.keyMap[ 46 ] = teWindowEvent::KeyCode::C;
    win.keyMap[ 32 ] = teWindowEvent::KeyCode::D;
    win.keyMap[ 18 ] = teWindowEvent::KeyCode::E;
    win.keyMap[ 33 ] = teWindowEvent::KeyCode::F;
    win.keyMap[ 34 ] = teWindowEvent::KeyCode::G;
    win.keyMap[ 35 ] = teWindowEvent::KeyCode::H;
    win.keyMap[ 23 ] = teWindowEvent::KeyCode::I;
    win.keyMap[ 36 ] = teWindowEvent::KeyCode::J;
    win.keyMap[ 37 ] = teWindowEvent::KeyCode::K;
    win.keyMap[ 38 ] = teWindowEvent::KeyCode::L;
    win.keyMap[ 50 ] = teWindowEvent::KeyCode::M;
    win.keyMap[ 49 ] = teWindowEvent::KeyCode::N;
    win.keyMap[ 24 ] = teWindowEvent::KeyCode::O;
    win.keyMap[ 25 ] = teWindowEvent::KeyCode::P;
    win.keyMap[ 16 ] = teWindowEvent::KeyCode::Q;
    win.keyMap[ 19 ] = teWindowEvent::KeyCode::R;
    win.keyMap[ 31 ] = teWindowEvent::KeyCode::S;
    win.keyMap[ 20 ] = teWindowEvent::KeyCode::T;
    win.keyMap[ 22 ] = teWindowEvent::KeyCode::U;
    win.keyMap[ 47 ] = teWindowEvent::KeyCode::V;
    win.keyMap[ 17 ] = teWindowEvent::KeyCode::W;
    win.keyMap[ 45 ] = teWindowEvent::KeyCode::X;
    win.keyMap[ 21 ] = teWindowEvent::KeyCode::Y;
    win.keyMap[ 44 ] = teWindowEvent::KeyCode::Z;

    win.keyMap[ 2 ] = teWindowEvent::KeyCode::N1;
    win.keyMap[ 3 ] = teWindowEvent::KeyCode::N2;
    win.keyMap[ 4 ] = teWindowEvent::KeyCode::N3;
    win.keyMap[ 5 ] = teWindowEvent::KeyCode::N4;
    win.keyMap[ 6 ] = teWindowEvent::KeyCode::N5;
    win.keyMap[ 7 ] = teWindowEvent::KeyCode::N6;
    win.keyMap[ 8 ] = teWindowEvent::KeyCode::N7;
    win.keyMap[ 9 ] = teWindowEvent::KeyCode::N8;
    win.keyMap[10 ] = teWindowEvent::KeyCode::N9;
    win.keyMap[11 ] = teWindowEvent::KeyCode::N0;
}

void teWindowGetSize( unsigned& outWidth, unsigned& outHeight )
{
    outWidth = win.width;
    outHeight = win.height;
}

void tePushWindowEvents()
{
    if (!win.gamePad.isActive)
    {
        return;
    }
    
    js_event j;

    while (read( win.gamePad.fd, &j, sizeof( js_event ) ) == sizeof( js_event ))
    {
        // Don't care if init or afterwards
        j.type &= ~JS_EVENT_INIT;
            
        if (j.type == JS_EVENT_BUTTON)
        {
            if (j.number == win.gamePad.buttonA && j.value > 0)
            {
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonA;
            }
            else if (j.number == win.gamePad.buttonB && j.value > 0)
            {
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonB;
            }
            else if (j.number == win.gamePad.buttonX && j.value > 0)
            {
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonX;
            }
            else if (j.number == win.gamePad.buttonY && j.value > 0)
            {
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonY;
            }
            else if (j.number == win.gamePad.buttonStart && j.value > 0)
            {
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonStart;
            }
            else if (j.number == win.gamePad.buttonBack && j.value > 0)
            {
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonBack;
            }
            else
            {
            }
        }
        else if (j.type == JS_EVENT_AXIS)
        {
            if (j.number == win.gamePad.leftThumbX)
            {
                const float x = ProcessGamePadStickValue( j.value, win.gamePad.deadZone );
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadLeftThumbState;
                win.events[ win.eventIndex ].gamePadThumbX = x;
                win.events[ win.eventIndex ].gamePadThumbY = win.gamePad.lastLeftThumbY;
                win.gamePad.lastLeftThumbX = x;
            }
            else if (j.number == win.gamePad.leftThumbY)
            {
                const float y = ProcessGamePadStickValue( j.value, win.gamePad.deadZone );
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadLeftThumbState;
                win.events[ win.eventIndex ].gamePadThumbX = win.gamePad.lastLeftThumbX;
                win.events[ win.eventIndex ].gamePadThumbY = -y;
                win.gamePad.lastLeftThumbY = -y;
            }
            else if (j.number == win.gamePad.rightThumbX)
            {
                const float x = ProcessGamePadStickValue( j.value, win.gamePad.deadZone );
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadRightThumbState;
                win.events[ win.eventIndex ].gamePadThumbX = x;
                win.events[ win.eventIndex ].gamePadThumbY = win.gamePad.lastRightThumbY;
                win.gamePad.lastRightThumbX = x;
            }
            else if (j.number == win.gamePad.rightThumbY)
            {
                const float y = ProcessGamePadStickValue( j.value, win.gamePad.deadZone );
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadRightThumbState;
                win.events[ win.eventIndex ].gamePadThumbX = win.gamePad.lastRightThumbX;
                win.events[ win.eventIndex ].gamePadThumbY = -y;
                win.gamePad.lastRightThumbY = -y;
            }
            else if (j.number == win.gamePad.dpadXaxis)
            {
                if (j.value > 0)
                {
                    IncEventIndex();
                    win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonDPadRight;
                }
                if (j.value < 0)
                {
                    IncEventIndex();
                    win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonDPadLeft;
                }
            }
            else if (j.number == win.gamePad.dpadYaxis)
            {
                if (j.value < 0)
                {
                    IncEventIndex();
                    win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonDPadUp;
                }
                if (j.value > 0)
                {
                    IncEventIndex();
                    win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonDPadDown;
                }
            }
        }
    }
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

void LoadAtoms()
{
    xcb_intern_atom_cookie_t wm_delete_window_cookie = xcb_intern_atom( connection, 0, 16, "WM_DELETE_WINDOW" );
    xcb_intern_atom_cookie_t wm_protocols_cookie = xcb_intern_atom( connection, 0, strlen("WM_PROTOCOLS"), "WM_PROTOCOLS" );

    xcb_flush( connection );
    xcb_intern_atom_reply_t* wm_delete_window_cookie_reply = xcb_intern_atom_reply( connection, wm_delete_window_cookie, nullptr );
    xcb_intern_atom_reply_t* wm_protocols_cookie_reply = xcb_intern_atom_reply( connection, wm_protocols_cookie, nullptr );

    win.wm_protocols = wm_protocols_cookie_reply->atom;
    win.wm_delete_window = wm_delete_window_cookie_reply->atom;
}

void* teCreateWindow( unsigned width, unsigned height, const char* title )
{
    win.display = XOpenDisplay( nullptr );

    if (!win.display)
    {
        printf( "Can't open display\n" );
        return nullptr;
    }
    
    const int default_screen = DefaultScreen( win.display );

    connection = XGetXCBConnection( win.display );
    LoadAtoms();

    if (!connection)
    {
        XCloseDisplay( win.display );
        printf( "Can't get xcb connection from display\n" );
        return nullptr;
    }
    
    XSetEventQueueOwner( win.display, XCBOwnsEventQueue );

    return nullptr;
}
