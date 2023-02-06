#include "file.h"
#include "window.h"
#include "renderer.h"
#include "shader.h"

int main()
{
	constexpr unsigned width = 1920;
	constexpr unsigned height = 1080;
	void* windowHandle = teCreateWindow( width, height, "Theseus Engine Hello" );
	teCreateRenderer( 1, windowHandle, width, height );

	teFile unlitVsFile = teLoadFile( "unlit_vs.spv" );
	teFile unlitFsFile = teLoadFile( "unlit_fs.spv" );
	teShader unlitShader = teCreateShader( unlitVsFile, unlitFsFile, "unlitVS", "unlitFS" );

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

			if (event.type == teWindowEvent::Type::KeyDown || event.type == teWindowEvent::Type::Close)
			{
				shouldQuit = true;
			}
		}

		teBeginFrame();
		teEndFrame();
	}

	return 0;
}
