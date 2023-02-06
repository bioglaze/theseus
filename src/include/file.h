#pragma once

struct teFile
{
    unsigned char* data = nullptr;
    unsigned size = 0;
    char path[ 260 ] = {};
};

teFile teLoadFile( const char* path );
