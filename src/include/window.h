#pragma once

struct teWindowEvent
{
    enum class Type
    {
        Empty, KeyDown, KeyUp, Close, Mouse1Down, Mouse1Up, Mouse2Down, Mouse2Up, MouseMove, MouseWheel
    };

    enum class KeyCode
    {
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z, AE, OE,
        Left, Right, Up, Down, Space, Escape, Enter, Backspace, Delete, Dot, Home, End,
        PageUp, PageDown, Plus, Minus, N0, N1, N2, N3, N4, N5, N6, N7, N8, N9, Comma,
        Less, None
    };

    Type type = Type::Empty;
    KeyCode keyCode = KeyCode::None;
    int x = 0;
    int y = 0;
    int wheelDelta = 0;
};

// @param width Width in pixels. Pass 0 to width and height for fullscreen.
// @param height Height in pixels. Pass 0 to width and height for fullscreen.
// @param title Window title.
// @return Window handle opaque pointer. On Windows its type is HWND.
void* teCreateWindow( unsigned width, unsigned height, const char* title );
void tePushWindowEvents();
// @return Type::Empty when no events remain in the event queue.
const teWindowEvent& tePopWindowEvent();
void teWindowGetSize( unsigned& outWidth, unsigned& outHeight );