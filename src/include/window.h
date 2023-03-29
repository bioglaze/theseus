#pragma once

struct teWindowEvent
{
    enum class Type
    {
        Empty, KeyDown, KeyUp, Close, Mouse1Down, Mouse1Up, Mouse2Down, Mouse2Up, MouseMove
    };

    enum class KeyCode
    {
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Left, Right, Up, Down, Space, Escape, Enter, None
    };

    Type type = Type::Empty;
    KeyCode keyCode = KeyCode::None;
    int x = 0;
    int y = 0;
};

// @param width Width in pixels.
// @param height Height in pixels.
// @param title Window title.
// @return Window handle opaque pointer. On Windows its type is HWND.
void* teCreateWindow( unsigned width, unsigned height, const char* title );
void tePushWindowEvents();
// @return Type::Empty when no events remain in the event queue.
const teWindowEvent& tePopWindowEvent();
