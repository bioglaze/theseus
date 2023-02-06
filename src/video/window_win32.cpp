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
};

WindowImpl win;

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

    const unsigned winWidth = width == 0 ? GetSystemMetrics( SM_CXSCREEN ) : width;
    const unsigned winHeight = height == 0 ? GetSystemMetrics( SM_CYSCREEN ) : height;

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
    wc.hIcon = static_cast< HICON >(LoadImage( nullptr, "theseus.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE) );

    RegisterClassEx( &wc );

    const int xPos = (GetSystemMetrics( SM_CXSCREEN ) - winWidth) / 2;
    const int yPos = (GetSystemMetrics( SM_CYSCREEN ) - winHeight) / 2;

    HWND hwnd = CreateWindowExA( fullscreen ? WS_EX_TOOLWINDOW | WS_EX_TOPMOST : 0,
        "WindowClass1", title,
        fullscreen ? WS_POPUP : (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU),
        xPos, yPos,
        winWidth, winHeight,
        nullptr, nullptr, hInstance, nullptr );

    ShowWindow( hwnd, SW_SHOW );
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
