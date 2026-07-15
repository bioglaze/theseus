#include "window.h"

void HandleEvent( const teWindowEvent& event );
void Init( unsigned width, unsigned height );
void Render();

int main()
{
    Init( 1920, 1080 );

    bool shouldQuit = false;

    while (!shouldQuit)
    {
        tePushWindowEvents();

        bool eventsHandled = false;

        while (!eventsHandled)
        {
            const teWindowEvent& event = tePopWindowEvent();

            if (event.type == teWindowEvent::Type::Empty)
            {
                eventsHandled = true;
            }

            if ((event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Escape) || event.type == teWindowEvent::Type::Close)
            {
                shouldQuit = true;
            }
            else
            {
                HandleEvent( event );
            }
        }

        Render();
    }
}
