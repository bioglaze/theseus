// Theseus engine editor
// Author: Timo Wiren
// Modified: 2024-09-01
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "window.h"
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "imgui.h"
#include "vec3.h"
#include "transform.h"

#if _WIN32
#include <Windows.h>

static LONGLONG PCFreq;

double GetMilliseconds()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter( &li );
    return (double)(li.QuadPart / (double)PCFreq);
}
#else
#include <time.h>

double GetMilliseconds()
{
    timespec spec;
    clock_gettime( CLOCK_MONOTONIC, &spec );
    return spec.tv_nsec / 1000000;
}
#endif

void InitSceneView( unsigned width, unsigned height, void* windowHandle, int uiScale );
void RenderSceneView();
unsigned SceneViewGetCameraIndex();
void SelectObject( unsigned x, unsigned y );
void DeleteSelectedObject();

struct InputState
{
    int x = 0;
    int y = 0;
    float deltaX = 0;
    float deltaY = 0;
    int lastMouseX = 0;
    int lastMouseY = 0;
    Vec3 moveDir;
    Vec3 gamepadMoveDir;
    bool isRightMouseDown = false;
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
    if (strstr( extension, "t3d" ))
    {
        ofn.lpstrFilter = "Theseus Mesh (.t3d)\0*.t3d\0All\0*.*\0";
    }
    else if (strstr( extension, "scene" ))
    {
        ofn.lpstrFilter = "Scene\0*.scene\0All\0*.*\0";
    }
    else if (strstr( extension, "wav" ))
    {
        ofn.lpstrFilter = "Audio\0*.wav\0All\0*.*\0";
    }

    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName( &ofn ) != FALSE)
    {
        strncpy( path, ofn.lpstrFile, 280 ); // 280 comes from sceneview.cpp: openFilePath
    }
}
#else
void GetOpenPath( char* path, const char* extension )
{
    FILE* f = nullptr;

    if (strstr( extension, "scene" ))
    {
        f = popen( "zenity --file-selection --file-filter=*.scene --title \"Load .scene file\"", "r" );
    }
    else if (strstr( extension, "t3d" ))
    {
        f = popen( "zenity --file-selection --file-filter=*.t3d --title \"Load .t3d file\"", "r" );
    }
    else if (strstr( extension, "wav" ))
    {
        f = popen( "zenity --file-selection --file-filter=*.wav --title \"Load .wav file\"", "r" );
    }
    else
    {
        f = popen( "zenity --file-selection --title \"Load .scene or .t3d file\"", "r" );
    }

    fgets( path, 1024, f );

    if (strlen( path ) > 0)
    {
        path[ strlen( path ) - 1 ] = 0;
    }
}
#endif

InputState inputParams;

bool HandleInput( unsigned /*width*/, unsigned /*height*/, double dt)
{
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
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Space)
        {
            io.AddInputCharacter( ' ' );
        }
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Left)
            io.AddKeyEvent( ImGuiKey::ImGuiKey_LeftArrow, true );
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::Left)
            io.AddKeyEvent( ImGuiKey::ImGuiKey_LeftArrow, false );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Right)
            io.AddKeyEvent( ImGuiKey::ImGuiKey_RightArrow, true );
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::Right)
            io.AddKeyEvent( ImGuiKey::ImGuiKey_RightArrow, false );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Backspace)
            io.AddKeyEvent( ImGuiKey::ImGuiKey_Backspace, true );
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::Backspace)
            io.AddKeyEvent( ImGuiKey::ImGuiKey_Backspace, false );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Delete)
        {
            if (!io.WantCaptureKeyboard)
            {
                DeleteSelectedObject();
                continue;
            }
            io.AddKeyEvent( ImGuiKey::ImGuiKey_Delete, true );
        }
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::Delete)
        {
            io.AddKeyEvent( ImGuiKey::ImGuiKey_Delete, false );
        }
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::N0)
            io.AddInputCharacter( '0' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::N1)
            io.AddInputCharacter( '1' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::N2)
            io.AddInputCharacter( '2' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::N3)
            io.AddInputCharacter( '3' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::N4)
            io.AddInputCharacter( '4' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::N5)
            io.AddInputCharacter( '5' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::N6)
            io.AddInputCharacter( '6' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::N7)
            io.AddInputCharacter( '7' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::N8)
            io.AddInputCharacter( '8' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::N9)
            io.AddInputCharacter( '9' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::B)
            io.AddInputCharacter( 'b' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::C)
            io.AddInputCharacter( 'c' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::F)
            io.AddInputCharacter( 'f' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::G)
            io.AddInputCharacter( 'g' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::H)
            io.AddInputCharacter( 'h' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::I)
            io.AddInputCharacter( 'i' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::J)
            io.AddInputCharacter( 'j' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::K)
            io.AddInputCharacter( 'k' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::L)
            io.AddInputCharacter( 'l' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::M)
            io.AddInputCharacter( 'm' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::N)
            io.AddInputCharacter( 'n' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::O)
            io.AddInputCharacter( 'o' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::P)
            io.AddInputCharacter( 'p' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::R)
            io.AddInputCharacter( 'r' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Dot)
            io.AddInputCharacter( '.' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Minus)
            io.AddInputCharacter( '-' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Plus)
            io.AddInputCharacter( '+' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Comma)
            io.AddInputCharacter( ',' );

        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::S)
        {
            if (!io.WantCaptureKeyboard)
                inputParams.moveDir.z = -0.5f;
            io.AddInputCharacter( 's' );
        }
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::S)
            inputParams.moveDir.z = 0;
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::T)
            io.AddInputCharacter( 't' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::U)
            io.AddInputCharacter( 'u' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::V)
            io.AddInputCharacter( 'v' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::X)
            io.AddInputCharacter( 'x' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Y)
            io.AddInputCharacter( 'y' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Z)
            io.AddInputCharacter( 'z' );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Less)
            io.AddInputCharacter( '<' );
        //else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::AE)
        //    io.AddInputCharactersUTF8( "ä" );
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::W)
        {
            if (!io.WantCaptureKeyboard)
                inputParams.moveDir.z = 0.5f;
            io.AddInputCharacter( 'w' );
        }
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::W)
            inputParams.moveDir.z = 0;
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::A)
        {
            if (!io.WantCaptureKeyboard)
                inputParams.moveDir.x = 0.5f;
            io.AddInputCharacter( 'a' );            
        }
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::A)
            inputParams.moveDir.x = 0;
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::D)
        {
            if (!io.WantCaptureKeyboard && (event.keyModifiers & (unsigned)teWindowEvent::KeyModifier::Control))
            {
                printf( "TODO: duplicate\n" );
            }
            else if (!io.WantCaptureKeyboard)
                inputParams.moveDir.x = -0.5f;
            io.AddInputCharacter( 'd' );
        }
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::D)
        {
            inputParams.moveDir.x = 0;
        }
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::Q)
        {
            if (!io.WantCaptureKeyboard)
                inputParams.moveDir.y = 0.5f;
            io.AddInputCharacter( 'q' );
        }
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::Q)
        {
            inputParams.moveDir.y = 0;
        }
        else if (event.type == teWindowEvent::Type::KeyDown && event.keyCode == teWindowEvent::KeyCode::E)
        {
            if (!io.WantCaptureKeyboard)
                inputParams.moveDir.y = -0.5f;
            io.AddInputCharacter( 'e' );
        }
        else if (event.type == teWindowEvent::Type::KeyUp && event.keyCode == teWindowEvent::KeyCode::E)
        {
            inputParams.moveDir.y = 0;
        }
        else if (event.type == teWindowEvent::Type::GamePadButtonA)
        {
            printf("gamepad button a\n");
        }
        else if (event.type == teWindowEvent::Type::GamePadLeftThumbState)
        {
            inputParams.gamepadMoveDir.z = event.gamePadThumbY;
            inputParams.gamepadMoveDir.x = -event.gamePadThumbX;
        }
        else if (event.type == teWindowEvent::Type::GamePadRightThumbState)
        {
            teTransformOffsetRotate( SceneViewGetCameraIndex(), Vec3( 0, 1, 0 ), -event.gamePadThumbX / 50.0f * (float)dt );
            teTransformOffsetRotate( SceneViewGetCameraIndex(), Vec3( 1, 0, 0 ), event.gamePadThumbY / 50.0f * (float)dt );
        }
        else if (event.type == teWindowEvent::Type::Mouse1Down)
        {
            inputParams.x = event.x;
            inputParams.y = event.y;
            inputParams.isRightMouseDown = true;
            inputParams.lastMouseX = inputParams.x;
            inputParams.lastMouseY = inputParams.y;
            inputParams.deltaX = 0;
            inputParams.deltaY = 0;

            io.AddMouseButtonEvent( 0, true );
        }
        else if (event.type == teWindowEvent::Type::Mouse1Up)
        {
            inputParams.x = event.x;
            inputParams.y = event.y;
            inputParams.isRightMouseDown = false;
            inputParams.deltaX = 0;
            inputParams.deltaY = 0;
            inputParams.lastMouseX = inputParams.x;
            inputParams.lastMouseY = inputParams.y;

            io.AddMouseButtonEvent( 0, false );
            
            if (!io.WantCaptureKeyboard)
            {
                SelectObject( event.x, event.y );
            }
        }
        else if (event.type == teWindowEvent::Type::Mouse2Down)
        {
            inputParams.x = event.x;
            inputParams.y = event.y;
            inputParams.isRightMouseDown = true;
            inputParams.lastMouseX = inputParams.x;
            inputParams.lastMouseY = inputParams.y;
            inputParams.deltaX = 0;
            inputParams.deltaY = 0;
            
            io.AddMouseButtonEvent( 1, true );

            //printf( "mouse 2 down. want capture mouse: %d\n", io.WantCaptureMouse );
        }
        else if (event.type == teWindowEvent::Type::Mouse2Up)
        {
            inputParams.x = event.x;
            inputParams.y = event.y;
            inputParams.isRightMouseDown = false;
            inputParams.deltaX = 0;
            inputParams.deltaY = 0;
            inputParams.lastMouseX = inputParams.x;
            inputParams.lastMouseY = inputParams.y;

            io.AddMouseButtonEvent( 1, false );
        }
        else if (event.type == teWindowEvent::Type::MouseMove)
        {
            inputParams.x = event.x;
            inputParams.y = event.y;
            inputParams.deltaX = float( inputParams.x - inputParams.lastMouseX );
            inputParams.deltaY = float( inputParams.y - inputParams.lastMouseY );
            inputParams.lastMouseX = inputParams.x;
            inputParams.lastMouseY = inputParams.y;

            if (inputParams.isRightMouseDown && !io.WantCaptureMouse)
            {
                teTransformOffsetRotate( SceneViewGetCameraIndex(), Vec3( 0, 1, 0 ), -inputParams.deltaX / 100.0f * (float)dt );
                teTransformOffsetRotate( SceneViewGetCameraIndex(), Vec3( 1, 0, 0 ), -inputParams.deltaY / 100.0f * (float)dt );
            }

            io.AddMousePosEvent( (float)inputParams.x, (float)inputParams.y );
        }
        else if (event.type == teWindowEvent::Type::MouseWheel)
        {
            inputParams.x = event.x;
            inputParams.y = event.y;
            inputParams.lastMouseX = inputParams.x;
            inputParams.lastMouseY = inputParams.y;
            printf( "wheel delta: %d\n", event.wheelDelta );
        }
    }

    Vec3 moveDir = inputParams.moveDir + inputParams.gamepadMoveDir * 0.005f;
    inputParams.gamepadMoveDir = Vec3( 0, 0, 0 );

    teTransformMoveForward( SceneViewGetCameraIndex(), moveDir.z * (float)dt * 0.5f );
    teTransformMoveRight( SceneViewGetCameraIndex(), moveDir.x * (float)dt * 0.5f );
    teTransformMoveUp( SceneViewGetCameraIndex(), moveDir.y * (float)dt * 0.5f );

    return true;
}

int main()
{
#ifdef _WIN32
    LARGE_INTEGER li;
    QueryPerformanceFrequency( &li );
    PCFreq = li.QuadPart / 1000;
#endif

    unsigned width = 1920 / 1;
    unsigned height = 1080 / 1;
    void* windowHandle = teCreateWindow( width, height, "Theseus Engine Editor" );
    teWindowGetSize( width, height );

    InitSceneView( width, height, windowHandle, 1 );

    double theTime = GetMilliseconds();
    double dt = 0;

    while (HandleInput( width, height, dt ))
    {
        double lastTime = theTime;
        theTime = GetMilliseconds();
        dt = theTime - lastTime;

        if (dt < 0)
        {
            dt = 0;
        }

        //printf( "%f\n", dt );

        RenderSceneView();
    }

    return 0;
}
