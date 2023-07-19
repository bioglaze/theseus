#include "file.h"
#include "te_stdlib.h"
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

#if __APPLE__
const char* GetFullPath( const char* fileName );
#else
const char* GetFullPath( const char* fileName )
{
    return fileName;
}
#endif

teFile teLoadFile( const char* path )
{
    teFile outFile;
    
    if (path && *path == 0)
    {
        return outFile;
    }

    FILE* file = fopen( GetFullPath( path ), "rb" );

    if (file)
    {    
        fseek( file, 0, SEEK_END );
        auto length = ftell( file );
        fseek( file, 0, SEEK_SET );
        outFile.data = (unsigned char*)teMalloc( length );
        outFile.size = (unsigned)length;

        for (unsigned i = 0; i < 260; ++i)
        {
            outFile.path[ i ] = path[ i ];

            if (path[ i ] == 0)
            {
                break;
            }
        }
        
        fread( outFile.data, 1, length, file );    
        fclose( file );
    }
    else
    {
        printf( "Could not open file %s\n", path );
    }
    
    return outFile;
}

struct Entry
{
    int hour = 0;
    int minute = 0;
    int second = 0;
    char path[ 260 ] = {};
    void(*updateFunc)(const char*) = nullptr;
};

Entry fileEntries[ 1000 ];
unsigned fileEntryCount = 0;

void RegisterFileForModifications( const teFile& file, void(*updateFunc)(const char*) )
{
    teAssert( fileEntryCount < 1000 );

    struct stat inode;

    for (unsigned i = 0; i < 260; ++i)
    {
        fileEntries[ fileEntryCount ].path[ i ] = file.path[ i ];
    }

    fileEntries[ fileEntryCount ].updateFunc = updateFunc;

    for (unsigned i = 0; i < fileEntryCount; ++i)
    {
#if _MSC_VER
        tm timeinfo;
        localtime_s( &timeinfo, &inode.st_mtime );
        const tm* timeinfo2 = &timeinfo;
#else
        const tm* timeinfo2 = localtime( &inode.st_mtime );
#endif
        fileEntries[ fileEntryCount ].hour = timeinfo2->tm_hour;
        fileEntries[ fileEntryCount ].minute = timeinfo2->tm_min;
        fileEntries[ fileEntryCount ].second = timeinfo2->tm_sec;
    }

    ++fileEntryCount;
}

void teHotReload()
{
    struct stat inode = {};

    for (unsigned i = 0; i < fileEntryCount; ++i)
    {
        if (stat( fileEntries[ i ].path, &inode ) != -1)
        {
#if _MSC_VER
            tm timeinfo;
            localtime_s( &timeinfo, &inode.st_mtime );
            const tm* timeinfo2 = &timeinfo;
#else
            const tm* timeinfo2 = localtime( &inode.st_mtime );
#endif
            if (timeinfo2->tm_hour != fileEntries[ i ].hour || timeinfo2->tm_min != fileEntries[ i ].minute || timeinfo2->tm_sec != fileEntries[ i ].second)
            {
                fileEntries[ i ].updateFunc( fileEntries[ i ].path );
                fileEntries[ i ].hour = timeinfo2->tm_hour;
                fileEntries[ i ].minute = timeinfo2->tm_min;
                fileEntries[ i ].second = timeinfo2->tm_sec;
            }
        }
    }
}
