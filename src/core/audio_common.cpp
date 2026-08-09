#include <stdint.h>
#include "audio.h"
#include "file.h"
#include "te_stdlib.h"

void LoadAudioWAV( const char* path, unsigned clipIndex );
void PlayAudioClip( unsigned clipIndex );

struct AudioClip
{
    unsigned internalIndex = 0; // index to audio_wasapi.cpp, audio_mac.cpp or audio_alsa.cpp struct
};

static constexpr unsigned MaxAudioClips = 1000;
static struct AudioClip gAudioClips[ MaxAudioClips ];
static unsigned gAudioClipIndex = 0;

struct AudioSource
{

};

static constexpr unsigned MaxAudioSources = 1000;
static struct AudioSource gAudioSources[ MaxAudioSources ];

struct WAVE
{
    uint8_t chunkID[ 4 ];
    uint32_t chunkSize;
    uint8_t format[ 4 ];

    // Format sub chunk
    uint8_t subchunk1ID[ 4 ];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t bytesPerSecond;
    uint16_t blockAlign;
    uint16_t bitsPerSample;

    // Data sub chunk
    uint8_t subchunk2ID[ 4 ]; // Data sub chunk ID.
    uint32_t subchunk2Size; // Data sub chunk size.
};

teAudioClip teLoadAudioClip( const struct teFile& wavFile )
{
    teAudioClip outClip;
    outClip.index = ++gAudioClipIndex;
    LoadAudioWAV( wavFile.path, outClip.index );

    return outClip;
}

void tePlayAudioClip( teAudioClip clip )
{
    PlayAudioClip( clip.index );
}

// FIXME: int16_t* is wrong for 8-bit data.
int16_t* LoadWAV( teFile& file, int& outSampleRate, int& outChannelCount, int& outFrameCount )
{
    if (!file.data)
    {
        return nullptr;
    }

    if (file.size < sizeof( WAVE ))
    {
        tePrint( "%s size is too small for a .wav file!\n", file.path );
        return nullptr;
    }

    WAVE* wav = reinterpret_cast< WAVE* >( file.data );

    if (wav->chunkID[ 0 ] != 'R' || wav->chunkID[ 1 ] != 'I' || wav->chunkID[ 2 ] != 'F' || wav->chunkID[ 3 ] != 'F')
    {
        tePrint( "%s doesn't contain valid header!\n", file.path );
        return nullptr;
    }

    if (wav->audioFormat != 1)
    {
        tePrint( "%s format is invalid!\n", file.path );
        return nullptr;
    }

    //printf( "chunk2: %c%c%c%c\n", wav->subchunk2ID[ 0 ], wav->subchunk2ID[ 1 ], wav->subchunk2ID[ 2 ], wav->subchunk2ID[ 3 ] ); // 16 for voice and alarms
    //printf( "subchunk1Size: %u\n", wav->subchunk1Size);

    if (wav->subchunk2ID[ 0 ] != 'L' || wav->subchunk2ID[ 1 ] != 'I' || wav->subchunk2ID[ 2 ] != 'S' || wav->subchunk2ID[ 3 ] != 'T')
    {
        tePrint( "LIST subchunk not handled yet in %s!\n", file.path );
    }

    if (wav->subchunk2ID[ 0 ] != 'd' || wav->subchunk2ID[ 1 ] != 'a' || wav->subchunk2ID[ 2 ] != 't' || wav->subchunk2ID[ 3 ] != 'a')
    {
        tePrint( "Warning! %s doesn't have an expected type of subchunk!\n", file.path );
    }

    outChannelCount = wav->numChannels;
    outSampleRate = wav->sampleRate;

    constexpr unsigned WAVE_FORMAT_PCM = 1;
    constexpr unsigned WAVE_FORMAT_IEEE_FLOAT = 0x0003;
    constexpr unsigned WAVE_FORMAT_EXTENSIBLE = 0xFFFE;

    if (wav->audioFormat == WAVE_FORMAT_PCM)
    {
        tePrint( "File is PCM\n" );
    }
    else if (wav->audioFormat == WAVE_FORMAT_IEEE_FLOAT)
    {
        tePrint( "File is WAVE_FORMAT_IEEE_FLOAT\n" );
    }
    else if (wav->audioFormat == WAVE_FORMAT_EXTENSIBLE)
    {
        tePrint( "File is WAVE_FORMAT_EXTENSIBLE\n" );
    }

    outFrameCount = wav->subchunk2Size / (wav->numChannels * sizeof( int16_t )); // FIXME: wrong for 8-bit

    tePrint( "sample rate: %d, channel count: %d, frame count: %d, bitsPerSample: %d\n", outSampleRate, outChannelCount, outFrameCount, wav->bitsPerSample );
tePrint( "sizeof WAVE: %u\n", sizeof( WAVE ));
    return reinterpret_cast< int16_t* >( file.data + sizeof( WAVE ) );
}
