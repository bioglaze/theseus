#pragma once

struct teFile
{
    unsigned char* data = nullptr;
    unsigned size = 0;
    char path[ 260 ] = {};
};

// @param path Path to the file to open.
// @return File. Caller is responsible for freeing teFile.data using free().
teFile teLoadFile( const char* path );
// Reloads shaders, textures etc. that have changed on disk.
void teHotReload();
