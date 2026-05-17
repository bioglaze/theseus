#include "file.h"
#include "te_stdlib.h"
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

teFile teLoadFile( const char* path )
{
    teFile outFile;
    
    if (!path || *path == 0)
    {
        return outFile;
    }

    FILE* file = fopen( path, "rb" );

    for (unsigned i = 0; i < 260; ++i)
    {
        outFile.path[ i ] = path[ i ];

        if (path[ i ] == 0)
        {
            break;
        }
    }

    if (file)
    {    
        fseek( file, 0, SEEK_END );
        auto length = ftell( file );
        fseek( file, 0, SEEK_SET );
        outFile.data = (unsigned char*)teMalloc( length );
        outFile.size = (unsigned)length;
        
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
        tm timeinfo = {};
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
            tm timeinfo = {};
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

#if _MSC_VER
struct DirectoryHandle
{
    WIN32_FIND_DATA ffd;
    HANDLE handle = INVALID_HANDLE_VALUE;
};

DirectoryHandle dirHandles[ 100 ];
unsigned dirHandleCount = 0;

unsigned teReadDirectory( const char* root )
{
    if (dirHandleCount == 99)
        dirHandleCount = 0;

    dirHandles[ dirHandleCount ].handle = FindFirstFile( root, &dirHandles[ dirHandleCount ].ffd );
    return dirHandleCount++;
}

bool teGetNextFile( unsigned handle, char** outPath )
{
    int n = FindNextFile( dirHandles[ handle ].handle, &dirHandles[ handle ].ffd );
    if (n != 0)
        *outPath = dirHandles[ handle ].ffd.cFileName;
    return n != 0;
}

void teCloseDirectory( unsigned handle )
{
    FindClose( dirHandles[ handle ].handle );
    
    if (dirHandleCount > 0)
        --dirHandleCount;
}
#else
#include <dirent.h>

struct DirectoryHandle
{
    DIR* dirFile = nullptr;
    dirent* handle = nullptr;
};

DirectoryHandle dirHandles[ 100 ];
unsigned dirHandleCount = 0;

unsigned teReadDirectory( const char* root )
{
    if (dirHandleCount == 99)
        dirHandleCount = 0;

    dirHandles[ dirHandleCount ].dirFile = opendir( root );

    if (dirHandles[ dirHandleCount ].dirFile == nullptr)
    {
        printf("teReadDirectory fail!\n");
    }

    return dirHandleCount++;
}

bool teGetNextFile( unsigned handle, char** outPath )
{
    teAssert( handle < 100 );

    if (dirHandles[ handle ].dirFile == nullptr)
    {
        return false;
    }

    dirHandles[ handle ].handle = readdir( dirHandles[ handle ].dirFile );

    if(dirHandles[ handle ].handle != nullptr)
    {
        *outPath = dirHandles[ handle ].handle->d_name;
        return true;
    }

    return false;
}

void teCloseDirectory( unsigned handle )
{
    teAssert( handle < 100 );

    if (dirHandles[ handle ].dirFile != nullptr)
    {
        closedir( dirHandles[ handle ].dirFile );
        dirHandles[ dirHandleCount ].dirFile = nullptr;
        dirHandles[ dirHandleCount ].handle = nullptr;
    }
}
#endif
