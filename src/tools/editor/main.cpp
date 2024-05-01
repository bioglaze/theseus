// Theseus engine editor
// Author: Timo Wiren
// Modified: 2024-05-01
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#if _WIN32
#include <Windows.h>
#endif

void InitSceneView( unsigned width, unsigned height );
void RenderSceneView();

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

int main()
{
    unsigned width = 1920 / 1;
    unsigned height = 1080 / 1;
    InitSceneView( width, height );
    RenderSceneView();

    return 0;
}
