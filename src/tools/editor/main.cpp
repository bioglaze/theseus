// Theseus engine editor
// Author: Timo Wiren
// Modified: 2024-05-05
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "window.h"
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "imgui.h"
#include "vec3.h"

#if _WIN32
#include <Windows.h>
#endif

void InitSceneView( unsigned width, unsigned height, void* windowHandle );
void RenderSceneView();

struct InputState
{
    int x = 0;
    int y = 0;
    float deltaX = 0;
    float deltaY = 0;
    int lastMouseX = 0;
    int lastMouseY = 0;
    Vec3 moveDir;
};

#if _MSC_VER
void GetOpenPath( char* path, const char* extension )
{
    OPENFILENAME ofn = {};
    TCHAR szFile[ 260 ] = {};

    ofn.lStructSize = sizeof( ofn );
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof( szFile );
    if (strstr( extension, "ae3d" ))
    {
        ofn.lpstrFilter = "Mesh\0*.ae3d\0All\0*.*\0";
    }
    else if (strstr( extension, "scene" ))
    {
        ofn.lpstrFilter = "Scene\0*.scene\0All\0*.*\0";
    }
    else if (strstr( extension, "wav" ))
    {
        ofn.lpstrFilter = "Audio\0*.wav;*.ogg\0All\0*.*\0";
    }

    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName( &ofn ) != FALSE)
    {
        strncpy( path, ofn.lpstrFile, 1024 );
    }
}
#else
void GetOpenPath( char* path, const char* extension )
{
    FILE* f = nullptr;

    if (strstr( extension, "scene" ))
    {
        f = popen( "zenity --file-selection --file-filter=*.scene --title \"Load .scene or .ae3d file\"", "r" );
    }
    else if (strstr( extension, "ae3d" ))
    {
        f = popen( "zenity --file-selection --file-filter=*.ae3d --title \"Load .scene or .ae3d file\"", "r" );
    }
    else if (strstr( extension, "wav" ))
    {
        f = popen( "zenity --file-selection --file-filter=*.wav --title \"Load .scene or .ae3d file\"", "r" );
    }
    else
    {
        f = popen( "zenity --file-selection --title \"Load .scene or .ae3d file\"", "r" );
    }

    fgets( path, 1024, f );

    if (strlen( path ) > 0)
    {
        path[ strlen( path ) - 1 ] = 0;
    }
}
#endif

bool HandleInput( unsigned width, unsigned height )
{
    InputState inputParams;
    bool isRightMouseDown = false;

    ImGuiIO& io = ImGui::GetIO();

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
            return false;
        }
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::S)
        {
            inputParams.moveDir.z = -0.5f;
        }
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::S)
        {
            inputParams.moveDir.z = 0;
        }
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::W)
        {
            inputParams.moveDir.z = 0.5f;
        }
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::W)
        {
            inputParams.moveDir.z = 0;
        }
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::A)
        {
            inputParams.moveDir.x = 0.5f;
            //io.AddKeyEvent( ImGuiKey_A, true );
        }
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::A)
        {
            inputParams.moveDir.x = 0;
        }
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::D)
        {
            inputParams.moveDir.x = -0.5f;
        }
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::D)
        {
            inputParams.moveDir.x = 0;
        }
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Q)
        {
            inputParams.moveDir.y = 0.5f;
        }
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::Q)
        {
            inputParams.moveDir.y = 0;
        }
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::E)
        {
            inputParams.moveDir.y = -0.5f;
        }
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::E)
        {
            inputParams.moveDir.y = 0;
        }
        else if (event.type == teWindowEvent::Type::Mouse1Down)
        {
            inputParams.x = event.x;
            inputParams.y = height - event.y;
            //isRightMouseDown = true;
            inputParams.lastMouseX = inputParams.x;
            inputParams.lastMouseY = inputParams.y;
            inputParams.deltaX = 0;
            inputParams.deltaY = 0;

            io.AddMouseButtonEvent( 0, true );
        }
        else if (event.type == teWindowEvent::Type::Mouse1Up)
        {
            inputParams.x = event.x;
            inputParams.y = height - event.y;
            //isRightMouseDown = false;
            inputParams.deltaX = 0;
            inputParams.deltaY = 0;
            inputParams.lastMouseX = inputParams.x;
            inputParams.lastMouseY = inputParams.y;

            io.AddMouseButtonEvent( 0, false );
        }
        else if (event.type == teWindowEvent::Type::Mouse2Down)
        {
            inputParams.x = event.x;
            inputParams.y = height - event.y;
            isRightMouseDown = true;
            inputParams.lastMouseX = inputParams.x;
            inputParams.lastMouseY = inputParams.y;
            inputParams.deltaX = 0;
            inputParams.deltaY = 0;
        }
        else if (event.type == teWindowEvent::Type::Mouse2Up)
        {
            inputParams.x = event.x;
            inputParams.y = height - event.y;
            isRightMouseDown = false;
            inputParams.deltaX = 0;
            inputParams.deltaY = 0;
            inputParams.lastMouseX = inputParams.x;
            inputParams.lastMouseY = inputParams.y;
        }
        else if (event.type == teWindowEvent::Type::MouseMove)
        {
            inputParams.x = event.x;
            inputParams.y = height - event.y;
            inputParams.deltaX = float( inputParams.x - inputParams.lastMouseX );
            inputParams.deltaY = float( inputParams.y - inputParams.lastMouseY );
            inputParams.lastMouseX = inputParams.x;
            inputParams.lastMouseY = inputParams.y;

            if (isRightMouseDown)
            {
                //teTransformOffsetRotate( sceneView.camera3d.index, Vec3( 0, 1, 0 ), -inputParams.deltaX / 100.0f * (float)dt );
                //teTransformOffsetRotate( sceneView.camera3d.index, Vec3( 1, 0, 0 ), -inputParams.deltaY / 100.0f * (float)dt );
            }

            io.AddMousePosEvent( (float)inputParams.x, (float)inputParams.y );
        }
    }

    return true;
}

int main()
{
    unsigned width = 1920 / 1;
    unsigned height = 1080 / 1;
    void* windowHandle = teCreateWindow( width, height, "Theseus Engine Editor" );
    teWindowGetSize( width, height );

    InitSceneView( width, height, windowHandle );

    while (HandleInput( width, height ))
    {
        RenderSceneView();
    }

    return 0;
}
