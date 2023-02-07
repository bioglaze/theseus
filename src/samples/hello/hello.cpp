#include "imgui.h"
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

	teFile unlitVsFile = teLoadFile( "shaders/unlit_vs.spv" );
	teFile unlitFsFile = teLoadFile( "shaders/unlit_fs.spv" );
	teShader unlitShader = teCreateShader( unlitVsFile, unlitFsFile, "unlitVS", "unlitFS" );

	ImGuiContext* imContext = ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

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

	delete[] unlitVsFile.data;
	delete[] unlitFsFile.data;

	ImGui::DestroyContext( imContext );

	return 0;
}
