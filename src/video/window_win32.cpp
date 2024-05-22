#include <Windows.h>
#if _DEBUG
#include <crtdbg.h>
#endif
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

static void InitKeyMap()
{
    for (unsigned keyIndex = 0; keyIndex < 256; ++keyIndex)
    {
        win.keyMap[ keyIndex ] = teWindowEvent::KeyCode::None;
    }

    win.keyMap[ VK_RETURN ] = teWindowEvent::KeyCode::Enter;
    win.keyMap[ VK_LEFT ] = teWindowEvent::KeyCode::Left;
    win.keyMap[ VK_UP ] = teWindowEvent::KeyCode::Up;
    win.keyMap[ VK_RIGHT ] = teWindowEvent::KeyCode::Right;
    win.keyMap[ VK_DOWN ] = teWindowEvent::KeyCode::Down;
    win.keyMap[ VK_ESCAPE ] = teWindowEvent::KeyCode::Escape;
    win.keyMap[ VK_SPACE ] = teWindowEvent::KeyCode::Space;
    win.keyMap[ VK_BACK ] = teWindowEvent::KeyCode::Backspace;
    win.keyMap[ VK_OEM_MINUS ] = teWindowEvent::KeyCode::Minus;
    win.keyMap[ VK_OEM_PLUS ] = teWindowEvent::KeyCode::Plus;
    win.keyMap[ VK_DELETE ] = teWindowEvent::KeyCode::Delete;
    win.keyMap[ VK_OEM_PERIOD ] = teWindowEvent::KeyCode::Dot;
    win.keyMap[ VK_HOME ] = teWindowEvent::KeyCode::Home;
    win.keyMap[ VK_END ] = teWindowEvent::KeyCode::End;
    win.keyMap[ VK_PRIOR ] = teWindowEvent::KeyCode::PageUp;
    win.keyMap[ VK_NEXT ] = teWindowEvent::KeyCode::PageDown;

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
    
    win.keyMap[48] = teWindowEvent::KeyCode::N0;
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

void IncEventIndex()
{
    if (win.eventIndex < EventStackSize - 1)
    {
        ++win.eventIndex;
    }
}

LRESULT CALLBACK WindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    switch( message )
    {
    case WM_KEYUP:
    case WM_SYSKEYUP:
        IncEventIndex();
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyUp;
        win.events[ win.eventIndex ].keyCode = win.keyMap[ (unsigned)wParam ];
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        IncEventIndex();
        win.events[ win.eventIndex ].type = teWindowEvent::Type::KeyDown;
        win.events[ win.eventIndex ].keyCode = win.keyMap[ (unsigned)wParam ];
        break;
    case WM_CLOSE:
        IncEventIndex();
        win.events[ win.eventIndex ].type = teWindowEvent::Type::Close;
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    {
        IncEventIndex();
        win.events[ win.eventIndex ].type = message == WM_LBUTTONDOWN ? teWindowEvent::Type::Mouse1Down : teWindowEvent::Type::Mouse1Up;
        win.events[ win.eventIndex ].x = LOWORD( lParam );
        win.events[ win.eventIndex ].y = win.windowHeightWithoutTitleBar - HIWORD( lParam );
    }
    break;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    {
        IncEventIndex();
        win.events[ win.eventIndex ].type = message == WM_RBUTTONDOWN ? teWindowEvent::Type::Mouse2Down : teWindowEvent::Type::Mouse2Up;
        win.events[ win.eventIndex ].x = LOWORD( lParam );
        win.events[ win.eventIndex ].y = win.windowHeightWithoutTitleBar - HIWORD( lParam );
    }
    break;
    case WM_MOUSEMOVE:
        IncEventIndex();
        win.events[ win.eventIndex ].type = teWindowEvent::Type::MouseMove;
        win.events[ win.eventIndex ].x = LOWORD( lParam );
        win.events[ win.eventIndex ].y = win.windowHeightWithoutTitleBar - HIWORD( lParam );
        break;

    default:
        break;
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

    return hwnd;
}

void tePushWindowEvents()
{
    MSG msg;

    while (PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ))
    {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
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
