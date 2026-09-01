#include "window.h"

void HandleEvent( const teWindowEvent& event );
void Init( unsigned width, unsigned height );
void Render();
void Tick();

int main()
{
    Init( 1920, 1080 );

    bool shouldQuit = false;

    while (!shouldQuit)
    {
        tePushWindowEvents();

        teWindowEvent* events = teGetWindowEvents();

        for (unsigned i = 0; i < teGetWindowEventCount(); ++i)
        {
            if ((events[ i ].type == teWindowEvent::Type::KeyDown && events[ i ].keyCode == teWindowEvent::KeyCode::Escape) || events[ i ].type == teWindowEvent::Type::Close)
            {
                shouldQuit = true;
            }
            else if (events[ i ].type != teWindowEvent::Type::Empty)
            {
                HandleEvent( events[ i ] );
            }
        }

        teClearWindowEvents();

        Tick();
        Render();
    }
}
