// @param swapInterval Swap interval. 0 means VSync is off, 1 means that it's the display's frame rate.
// @param windowHandle Opaque pointer to the window handle. Pass the value that's returned by teCreateWindow().
// @param width Width of the swap chain in pixels.
// @param height Height of the swap chain in pixels.
void teCreateRenderer( unsigned swapInterval, void* windowHandle, unsigned width, unsigned height );
void teBeginFrame();
void teEndFrame();
