#include <stdint.h>
#include "file.h"
#include "te_stdlib.h"

struct AudioSource
{

};

static constexpr unsigned MaxAudioSources = 10000;
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

    if (wav->numChannels == 1 && wav->bitsPerSample == 8)
    {
        //format = AL_FORMAT_MONO8;
    }
    else if (wav->numChannels == 2 && wav->bitsPerSample == 8)
    {
        //format = AL_FORMAT_STEREO8;
    }
    else if (wav->numChannels == 1 && wav->bitsPerSample == 16)
    {
        //format = AL_FORMAT_MONO16;
    }
    else if (wav->numChannels == 2 && wav->bitsPerSample == 16)
    {
        //format = AL_FORMAT_STEREO16;
    }
    else
    {
        tePrint( "%s doesn't have a valid format! Required channels: 1 or 2, bitsPerSample: 8 or 16. File's channels is %d and bitsPerSample is %d\n", file.path, wav->numChannels, wav->bitsPerSample );
        return nullptr;
    }

    outFrameCount = wav->subchunk2Size / (wav->numChannels * sizeof( uint16_t )); // FIXME: wrong for 8-bit

    tePrint( "sample rate: %d, channel count: %d, frame count: %d, bitsPerSample: %d\n", outSampleRate, outChannelCount, outFrameCount, wav->bitsPerSample );

    return reinterpret_cast< int16_t* >( file.data + sizeof( WAVE ));
}
