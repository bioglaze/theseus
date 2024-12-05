#include <Windows.h>
#include <xinput.h>
#if _DEBUG
#include <crtdbg.h>
#endif
#include "window.h"

typedef DWORD WINAPI x_input_get_state( DWORD dwUserIndex, XINPUT_STATE* pState );

DWORD WINAPI XInputGetStateStub( DWORD /*dwUserIndex*/, XINPUT_STATE* /*pState*/ )
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

static x_input_get_state* XInputGetState_ = XInputGetStateStub;

typedef DWORD WINAPI x_input_set_state( DWORD dwUserIndex, XINPUT_VIBRATION* pVibration );

DWORD WINAPI XInputSetStateStub( DWORD /*dwUserIndex*/, XINPUT_VIBRATION* /*pVibration*/ )
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

static x_input_set_state* XInputSetState_ = XInputSetStateStub;

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
    bool isGamePadConnected = false;
    int gamePadIndex = 0;
};

WindowImpl win;

void InitGamePad()
{
    HMODULE XInputLibrary = LoadLibraryA( "xinput1_4.dll" );

    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibraryA( "xinput9_1_0.dll" );
    }

    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibraryA( "xinput1_3.dll" );
    }

    if (XInputLibrary)
    {
        XInputGetState_ = (x_input_get_state*)GetProcAddress( XInputLibrary, "XInputGetState" );
        XInputSetState_ = (x_input_set_state*)GetProcAddress( XInputLibrary, "XInputSetState" );

        XINPUT_STATE controllerState;

        win.isGamePadConnected = XInputGetState_( 0, &controllerState ) == ERROR_SUCCESS;

        if (!win.isGamePadConnected)
        {
            win.isGamePadConnected = XInputGetState_( 1, &controllerState ) == ERROR_SUCCESS;
            win.gamePadIndex = 1;
        }
    }
    else
    {
    }
}

static float ProcessGamePadStickValue( SHORT value, SHORT deadZoneThreshold )
{
    float result = 0;

    if (value < -deadZoneThreshold)
    {
        result = (float)((value + deadZoneThreshold) / (32768.0f - deadZoneThreshold));
    }
    else if (value > deadZoneThreshold)
    {
        result = (float)((value - deadZoneThreshold) / (32767.0f - deadZoneThreshold));
    }

    return result;
}

static void InitKeyMap()
{
    for (unsigned keyIndex = 0; keyIndex < 256; ++keyIndex)
    {
        win.keyMap[ keyIndex ] = teWindowEvent::KeyCode::None;
    }

    win.keyMap[ VK_RETURN ]     = teWindowEvent::KeyCode::Enter;
    win.keyMap[ VK_LEFT ]       = teWindowEvent::KeyCode::Left;
    win.keyMap[ VK_UP ]         = teWindowEvent::KeyCode::Up;
    win.keyMap[ VK_RIGHT ]      = teWindowEvent::KeyCode::Right;
    win.keyMap[ VK_DOWN ]       = teWindowEvent::KeyCode::Down;
    win.keyMap[ VK_ESCAPE ]     = teWindowEvent::KeyCode::Escape;
    win.keyMap[ VK_SPACE ]      = teWindowEvent::KeyCode::Space;
    win.keyMap[ VK_BACK ]       = teWindowEvent::KeyCode::Backspace;
    win.keyMap[ VK_OEM_MINUS ]  = teWindowEvent::KeyCode::Minus;
    win.keyMap[ VK_OEM_PLUS ]   = teWindowEvent::KeyCode::Plus;
    win.keyMap[ VK_DELETE ]     = teWindowEvent::KeyCode::Delete;
    win.keyMap[ VK_OEM_PERIOD ] = teWindowEvent::KeyCode::Dot;
    win.keyMap[ VK_HOME ]       = teWindowEvent::KeyCode::Home;
    win.keyMap[ VK_END ]        = teWindowEvent::KeyCode::End;
    win.keyMap[ VK_PRIOR ]      = teWindowEvent::KeyCode::PageUp;
    win.keyMap[ VK_NEXT ]       = teWindowEvent::KeyCode::PageDown;
    win.keyMap[ VK_OEM_COMMA ]  = teWindowEvent::KeyCode::Comma;
    win.keyMap[ VK_TAB ]        = teWindowEvent::KeyCode::Tab;

    win.keyMap[ 65 ] = teWindowEvent::KeyCode::A;
    win.keyMap[ 66 ] = teWindowEvent::KeyCode::B;
    win.keyMap[ 67 ] = teWindowEvent::KeyCode::C;
    win.keyMap[ 68 ] = teWindowEvent::KeyCode::D;
    win.keyMap[ 69 ] = teWindowEvent::KeyCode::E;
    win.keyMap[ 70 ] = teWindowEvent::KeyCode::F;
    win.keyMap[ 71 ] = teWindowEvent::KeyCode::G;
    win.keyMap[ 72 ] = teWindowEvent::KeyCode::H;
    win.keyMap[ 73 ] = teWindowEvent::KeyCode::I;
    win.keyMap[ 74 ] = teWindowEvent::KeyCode::J;
    win.keyMap[ 75 ] = teWindowEvent::KeyCode::K;
    win.keyMap[ 76 ] = teWindowEvent::KeyCode::L;
    win.keyMap[ 77 ] = teWindowEvent::KeyCode::M;
    win.keyMap[ 78 ] = teWindowEvent::KeyCode::N;
    win.keyMap[ 79 ] = teWindowEvent::KeyCode::O;
    win.keyMap[ 80 ] = teWindowEvent::KeyCode::P;
    win.keyMap[ 81 ] = teWindowEvent::KeyCode::Q;
    win.keyMap[ 82 ] = teWindowEvent::KeyCode::R;
    win.keyMap[ 83 ] = teWindowEvent::KeyCode::S;
    win.keyMap[ 84 ] = teWindowEvent::KeyCode::T;
    win.keyMap[ 85 ] = teWindowEvent::KeyCode::U;
    win.keyMap[ 86 ] = teWindowEvent::KeyCode::V;
    win.keyMap[ 87 ] = teWindowEvent::KeyCode::W;
    win.keyMap[ 88 ] = teWindowEvent::KeyCode::X;
    win.keyMap[ 89 ] = teWindowEvent::KeyCode::Y;
    win.keyMap[ 90 ] = teWindowEvent::KeyCode::Z;
    win.keyMap[ 222 ] = teWindowEvent::KeyCode::AE;
    win.keyMap[ 192 ] = teWindowEvent::KeyCode::OE;
    win.keyMap[ 226 ] = teWindowEvent::KeyCode::Less;
    
    win.keyMap[ 48 ] = teWindowEvent::KeyCode::N0;
    win.keyMap[ 49 ] = teWindowEvent::KeyCode::N1;
    win.keyMap[ 50 ] = teWindowEvent::KeyCode::N2;
    win.keyMap[ 51 ] = teWindowEvent::KeyCode::N3;
    win.keyMap[ 52 ] = teWindowEvent::KeyCode::N4;
    win.keyMap[ 53 ] = teWindowEvent::KeyCode::N5;
    win.keyMap[ 54 ] = teWindowEvent::KeyCode::N6;
    win.keyMap[ 55 ] = teWindowEvent::KeyCode::N7;
    win.keyMap[ 56 ] = teWindowEvent::KeyCode::N8;
    win.keyMap[ 57 ] = teWindowEvent::KeyCode::N9;
}

bool IncEventIndex()
{
    if (win.eventIndex < EventStackSize - 1)
    {
        ++win.eventIndex;
        return true;
    }

    return false;
}

LRESULT CALLBACK WindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    bool wasHandled = false;

    switch( message )
    {
    case WM_KEYUP:
    case WM_SYSKEYUP:
        wasHandled = IncEventIndex();
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = win.keyMap[ (unsigned)wParam ];
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        wasHandled = IncEventIndex();
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = win.keyMap[ (unsigned)wParam ];
        break;
    case WM_CLOSE:
        wasHandled = IncEventIndex();
        win.events[ win.eventIndex ].type = teWindowEvent::Type::Close;
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    {
        wasHandled = IncEventIndex();
        win.events[ win.eventIndex ].type = message == WM_LBUTTONDOWN ? teWindowEvent::Type::Mouse1Down : teWindowEvent::Type::Mouse1Up;
        win.events[ win.eventIndex ].x = LOWORD( lParam );
        win.events[ win.eventIndex ].y = HIWORD( lParam );
    }
    break;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    {
        wasHandled = IncEventIndex();
        win.events[ win.eventIndex ].type = message == WM_RBUTTONDOWN ? teWindowEvent::Type::Mouse2Down : teWindowEvent::Type::Mouse2Up;
        win.events[ win.eventIndex ].x = LOWORD( lParam );
        win.events[ win.eventIndex ].y = HIWORD( lParam );
    }
    break;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    {
        wasHandled = IncEventIndex();
        win.events[ win.eventIndex ].type = message == WM_MBUTTONDOWN ? teWindowEvent::Type::Mouse3Down : teWindowEvent::Type::Mouse3Up;
        win.events[ win.eventIndex ].x = LOWORD( lParam );
        win.events[ win.eventIndex ].y = HIWORD( lParam );
    }
    break;
    case WM_MOUSEMOVE:
        wasHandled = IncEventIndex();
        win.events[ win.eventIndex ].type = teWindowEvent::Type::MouseMove;
        win.events[ win.eventIndex ].x = LOWORD( lParam );
        win.events[ win.eventIndex ].y = HIWORD( lParam );
        break;
    case WM_MOUSEWHEEL:
        wasHandled = IncEventIndex();
        win.events[ win.eventIndex ].type = teWindowEvent::Type::MouseWheel;
        win.events[ win.eventIndex ].x = LOWORD( lParam );
        win.events[ win.eventIndex ].y = HIWORD( lParam );
        win.events[ win.eventIndex ].wheelDelta = GET_WHEEL_DELTA_WPARAM( wParam );
        break;
    default:
        break;
    }

    if (wasHandled)
    {
        win.events[ win.eventIndex ].keyModifiers = 0;

        if ((GetKeyState( VK_CONTROL ) & 0x8000) != 0)
        {
            win.events[ win.eventIndex ].keyModifiers = (unsigned)teWindowEvent::KeyModifier::Control;
        }
        if ((GetKeyState( VK_SHIFT ) & 0x8000) != 0)
        {
            win.events[ win.eventIndex ].keyModifiers |= (unsigned)teWindowEvent::KeyModifier::Shift;
        }
        if ((GetKeyState( VK_MENU ) & 0x8000) != 0)
        {
            win.events[ win.eventIndex ].keyModifiers |= (unsigned)teWindowEvent::KeyModifier::Alt;
        }
    }

    return DefWindowProc( hWnd, message, wParam, lParam );
}

void* teCreateWindow( unsigned width, unsigned height, const char* title )
{
#if _DEBUG
    _CrtSetDbgFlag( _CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF );
    _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
    _CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
#endif

    win.width = width == 0 ? GetSystemMetrics( SM_CXSCREEN ) : width;
    win.height = height == 0 ? GetSystemMetrics( SM_CYSCREEN ) : height;

    const HINSTANCE hInstance = GetModuleHandle(nullptr);
    const bool fullscreen = (width == 0 && height == 0);

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof( WNDCLASSEX );
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor( nullptr, IDC_ARROW );
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = "WindowClass1";
    wc.hIcon = static_cast< HICON >(LoadImage( nullptr, "assets/textures/theseus.ico", IMAGE_ICON, 128, 128, LR_LOADFROMFILE) );

    RegisterClassEx( &wc );

    const int xPos = (GetSystemMetrics( SM_CXSCREEN ) - win.width) / 2;
    const int yPos = (GetSystemMetrics( SM_CYSCREEN ) - win.height) / 2;

    HWND hwnd = CreateWindowExA( fullscreen ? WS_EX_TOOLWINDOW | WS_EX_TOPMOST : 0,
        "WindowClass1", title,
        fullscreen ? WS_POPUP : (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU),
        xPos, yPos,
        win.width, win.height,
        nullptr, nullptr, hInstance, nullptr );

    ShowWindow( hwnd, SW_SHOW );

    RECT rect = {};
    GetClientRect( hwnd, &rect );
    win.windowWidthWithoutTitleBar = rect.right;
    win.windowHeightWithoutTitleBar = rect.bottom;

    InitKeyMap();
    InitGamePad();

    return hwnd;
}

void PumpGamePadEvents()
{
    XINPUT_STATE controllerState;

    if (XInputGetState_( win.gamePadIndex, &controllerState ) == ERROR_SUCCESS)
    {
        XINPUT_GAMEPAD* pad = &controllerState.Gamepad;

        float avgX = ProcessGamePadStickValue( pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE );
        float avgY = ProcessGamePadStickValue( pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE );

        IncEventIndex();
        win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadLeftThumbState;
        win.events[ win.eventIndex ].gamePadThumbX = avgX;
        win.events[ win.eventIndex ].gamePadThumbY = avgY;

        avgX = ProcessGamePadStickValue( pad->sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE );
        avgY = ProcessGamePadStickValue( pad->sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE );

        IncEventIndex();
        win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadRightThumbState;
        win.events[ win.eventIndex ].gamePadThumbX = avgX;
        win.events[ win.eventIndex ].gamePadThumbY = avgY;

        if ((pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0)
        {
            IncEventIndex();
            win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonDPadUp;
        }
        if ((pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0)
        {
            IncEventIndex();
            win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonDPadDown;
        }
        if ((pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0)
        {
            IncEventIndex();
            win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonDPadLeft;
        }
        if ((pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0)
        {
            IncEventIndex();
            win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonDPadRight;
        }
        if ((pad->wButtons & XINPUT_GAMEPAD_A) != 0)
        {
            IncEventIndex();
            win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonA;
        }
        if ((pad->wButtons & XINPUT_GAMEPAD_B) != 0)
        {
            IncEventIndex();
            win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonB;
        }
        if ((pad->wButtons & XINPUT_GAMEPAD_X) != 0)
        {
            IncEventIndex();
            win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonX;
        }
        if ((pad->wButtons & XINPUT_GAMEPAD_Y) != 0)
        {
            IncEventIndex();
            win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonY;
        }
        if ((pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0)
        {
            IncEventIndex();
            win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonLeftShoulder;
        }
        if ((pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0)
        {
            IncEventIndex();
            win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonRightShoulder;
        }
        if ((pad->wButtons & XINPUT_GAMEPAD_START) != 0)
        {
            IncEventIndex();
            win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonStart;
        }
        if ((pad->wButtons & XINPUT_GAMEPAD_BACK) != 0)
        {
            IncEventIndex();
            win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonBack;
        }
    }
}

void tePushWindowEvents()
{
    MSG msg;

    while (PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ))
    {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

    if (win.isGamePadConnected)
    {
        PumpGamePadEvents();
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

void teWindowGetSize( unsigned& outWidth, unsigned& outHeight )
{
    outWidth = win.windowWidthWithoutTitleBar;
    outHeight = win.windowHeightWithoutTitleBar;
}
