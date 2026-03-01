#include "window.h"
#include "te_stdlib.h"
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_ewmh.h>
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
    xcb_key_symbols_t*    keySymbols = nullptr;
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

teWindowEvent::KeyCode GetKeycode( uint32_t xcbKey )
{
    switch( xcbKey )
    {
    case 97: return teWindowEvent::KeyCode::A;
    case 98: return teWindowEvent::KeyCode::B;
    case 99: return teWindowEvent::KeyCode::C;
    case 100: return teWindowEvent::KeyCode::D;
    case 101: return teWindowEvent::KeyCode::E;
    case 102: return teWindowEvent::KeyCode::F;
    case 103: return teWindowEvent::KeyCode::G;
    case 104: return teWindowEvent::KeyCode::H;
    case 105: return teWindowEvent::KeyCode::I;
    case 106: return teWindowEvent::KeyCode::J;
    case 107: return teWindowEvent::KeyCode::K;
    case 108: return teWindowEvent::KeyCode::L;
    case 109: return teWindowEvent::KeyCode::M;
    case 110: return teWindowEvent::KeyCode::N;
    case 111: return teWindowEvent::KeyCode::O;
    case 112: return teWindowEvent::KeyCode::P;
    case 113: return teWindowEvent::KeyCode::Q;
    case 114: return teWindowEvent::KeyCode::R;
    case 115: return teWindowEvent::KeyCode::S;
    case 116: return teWindowEvent::KeyCode::T;
    case 117: return teWindowEvent::KeyCode::U;
    case 118: return teWindowEvent::KeyCode::V;
    case 119: return teWindowEvent::KeyCode::W;
    case 120: return teWindowEvent::KeyCode::X;
    case 121: return teWindowEvent::KeyCode::Y;
    case 122: return teWindowEvent::KeyCode::Z;
    case 32: return teWindowEvent::KeyCode::Space;
    case 65293: return teWindowEvent::KeyCode::Enter;
    case 65361: return teWindowEvent::KeyCode::Left;
    case 65362: return teWindowEvent::KeyCode::Up;
    case 65363: return teWindowEvent::KeyCode::Right;
    case 65364: return teWindowEvent::KeyCode::Down;
    case 65307: return teWindowEvent::KeyCode::Escape;
    default: return teWindowEvent::KeyCode::A;
    }
}

void teWindowGetSize( unsigned& outWidth, unsigned& outHeight )
{
    outWidth = win.width;
    outHeight = win.height;
}

void tePushWindowEvents()
{
    xcb_generic_event_t* event;
    
    while ((event = xcb_poll_for_event( connection )))
    {
        if (win.eventIndex >= 14)
        {
            free( event );
            return;
        }
        
        const uint8_t responseType = event->response_type & ~0x80;

        if (responseType == XCB_BUTTON_PRESS || responseType == XCB_BUTTON_RELEASE)
        {
            xcb_button_press_event_t* bp = (xcb_button_press_event_t *)event;
            IncEventIndex();
            
            if (bp->detail == 2)
            {
                win.events[ win.eventIndex ].type = responseType == XCB_BUTTON_RELEASE ? teWindowEvent::Type::Mouse3Up : teWindowEvent::Type::Mouse3Down;
            }
            else if (bp->detail == 3)
            {
                win.events[ win.eventIndex ].type = responseType == XCB_BUTTON_RELEASE ? teWindowEvent::Type::Mouse2Up : teWindowEvent::Type::Mouse2Down;
            }
            else
            {
                win.events[ win.eventIndex ].type = responseType == XCB_BUTTON_RELEASE ? teWindowEvent::Type::Mouse1Up : teWindowEvent::Type::Mouse1Down;
            }
            
            win.events[ win.eventIndex ].x = bp->event_x;
            win.events[ win.eventIndex ].y = bp->event_y;
        }
        else if (responseType == XCB_KEY_PRESS)
        {
            IncEventIndex();
            xcb_key_press_event_t* kp = (xcb_key_press_event_t *)event;
            const xcb_keysym_t keysym = xcb_key_symbols_get_keysym( win.keySymbols, kp->detail, 0 );
            
            win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
            win.events[ win.eventIndex ].keyCode = GetKeycode( keysym );
        }
        else if (responseType == XCB_KEY_RELEASE)
        {
            IncEventIndex();
            xcb_key_press_event_t* kp = (xcb_key_press_event_t *)event;
            const xcb_keysym_t keysym = xcb_key_symbols_get_keysym( win.keySymbols, kp->detail, 0 );

            win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
            win.events[ win.eventIndex ].keyCode = GetKeycode( keysym );
        }
        else if (responseType == XCB_MOTION_NOTIFY)
        {
            IncEventIndex();
            xcb_motion_notify_event_t* motion = (xcb_motion_notify_event_t *)event;
            win.events[ win.eventIndex ].type = teWindowEvent::Type::MouseMove;
            win.events[ win.eventIndex ].x = motion->event_x;
            win.events[ win.eventIndex ].y = motion->event_y;
        }
        
        free( event );
    }

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

void* teCreateWindow( unsigned width, unsigned height, const char* title )
{
    connection = xcb_connect( nullptr, nullptr );

    if (xcb_connection_has_error( connection ))
    {
        printf( "Can't get xcb connection from display\n" );
        return nullptr;
    }
    
    xcb_screen_t* s = xcb_setup_roots_iterator( xcb_get_setup( connection ) ).data;
    window = xcb_generate_id( connection );
    win.width = width == 0 ? s->width_in_pixels : width;
    win.height = height == 0 ? s->height_in_pixels : height;
    win.keySymbols = xcb_key_symbols_alloc( connection );

    const unsigned mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    const unsigned eventMask = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                               XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION;
    const unsigned values[ 2 ] { s->white_pixel, eventMask };
    
    xcb_create_window( connection, s->root_depth, window, s->root,
                       10, 10,
                       win.width,
                       win.height,
                       1,
                       XCB_WINDOW_CLASS_INPUT_OUTPUT, s->root_visual,
                       mask, values );

    xcb_map_window( connection, window );
    xcb_flush( connection );

    xcb_size_hints_t hints = {};
    xcb_icccm_size_hints_set_min_size( &hints, win.width, win.height );
    xcb_icccm_size_hints_set_max_size( &hints, win.width, win.height );
    xcb_icccm_set_wm_size_hints( connection, window, XCB_ATOM_WM_NORMAL_HINTS, &hints );

    xcb_change_property( connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen( title ), title );
    xcb_change_property( connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, sizeof("Theseus""\0""Theseus"), "theseus\0Theseus" );

    xcb_ewmh_connection_t EWMH;
    xcb_intern_atom_cookie_t* EWMHCookie = xcb_ewmh_init_atoms( connection, &EWMH );

    if (!xcb_ewmh_init_atoms_replies( &EWMH, EWMHCookie, nullptr ))
    {
        return nullptr;
    }

    xcb_flush( connection );

    if (width == 0 && height == 0)
    {
        xcb_change_property( connection, XCB_PROP_MODE_REPLACE, window, EWMH._NET_WM_STATE, XCB_ATOM_ATOM, 32, 1, &(EWMH._NET_WM_STATE_FULLSCREEN) );
        
        xcb_generic_error_t* error;
        xcb_get_window_attributes_reply_t* reply = xcb_get_window_attributes_reply( connection,
                                                                                    xcb_get_window_attributes( connection,
                                                                                                               window ), &error );

        if (!reply)
        {
            return nullptr;
        }

        free( reply );
    }

    InitKeyMap();
    
    return nullptr;
}
