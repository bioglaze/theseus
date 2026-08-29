#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include "file.h"
#include "te_stdlib.h"
#include <math.h>

int16_t* LoadWAV( teFile& file, int& outSampleRate, int& outChannelCount, int& outFrameCount );

struct AudioDevice
{
    AudioComponent outputComp;
    AudioComponentInstance outputInstance;
    unsigned playingClipIndex = 0;
    unsigned wavPlaybackSample = 0;
};

AudioDevice gAudioDevice;

enum format_type
{
    FMT_S16_LE,
    FMT_S16_BE,
    FMT_S32_LE,
    FMT_S32_BE,
    FMT_FLOAT
};

struct CoreAudioFormatDescriptionMap
{
    enum format_type type;
    int bitsPerSample;
    int bytes_per_sample;
    unsigned int flags;
};

static struct CoreAudioFormatDescriptionMap formatMap[] =
{
    { FMT_S16_LE, 16, sizeof( int16_t ), kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked },
    { FMT_S16_BE, 16, sizeof( int16_t ), kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsPacked },
    { FMT_S32_LE, 32, sizeof( int32_t ), kAudioFormatFlagIsSignedInteger },
    { FMT_S32_BE, 32, sizeof( int32_t ), kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian },
    { FMT_FLOAT,  32, sizeof( float ),   kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved },
};

struct AudioClipInternal
{
    int channelCount = 0;
    int sampleRate = 0;
    int frameCount = 0;
    teFile wavFile;
    int16_t* data = nullptr;
};

AudioClipInternal audioClipInternals[ 1000 ]; // Indexed by audio_common.cpp audioClipIndex

OSStatus tone( void* inRef, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* timeStamp, UInt32 busNumber, UInt32 numberFrames, AudioBufferList* ioData )
{
    const int channel = 0;
    int16_t* buffer = (int16_t*)ioData->mBuffers[ channel ].mData;
    
    //printf("inNumberFrames: %u, data size: %u\n", numberFrames, audioClipInternals[ 1 ].wavFile.size );

    const int channelCount = audioClipInternals[ gAudioDevice.playingClipIndex ].channelCount;

    for (UInt32 frame = 0; frame < numberFrames; ++frame) 
    {
        buffer[ frame * channelCount + 0 ] = audioClipInternals[ gAudioDevice.playingClipIndex ].data[ gAudioDevice.wavPlaybackSample ];
        ++gAudioDevice.wavPlaybackSample;

        if (channelCount == 2)
        {
            buffer[ frame * channelCount + 1 ] = audioClipInternals[ gAudioDevice.playingClipIndex ].data[ gAudioDevice.wavPlaybackSample ];
            ++gAudioDevice.wavPlaybackSample;
        }
        
        if (gAudioDevice.wavPlaybackSample >= audioClipInternals[ gAudioDevice.playingClipIndex ].frameCount * channelCount)
        {
            gAudioDevice.wavPlaybackSample = 0;
        }
    }

    return noErr;
}

bool OpenAudio( enum format_type format, int rate, int chan, AURenderCallbackStruct* callback )
{
    struct CoreAudioFormatDescriptionMap* m = nullptr;

    for (unsigned i = 0; i < sizeof( formatMap ) / sizeof( formatMap[ 0 ] ); ++i)
    {
        if (formatMap[ i ].type == format)
        {
            m = &formatMap[ i ];
            break;
        }
    }

    if (!m)
    {
        tePrint( "The requested audio format %d is unsupported.\n", format );
        return false;
    }

    AudioStreamBasicDescription streamFormat = {};
    streamFormat.mSampleRate = rate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = m->flags;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mChannelsPerFrame = chan;
    streamFormat.mBitsPerChannel = m->bitsPerSample;
    streamFormat.mBytesPerPacket = (m->flags & kAudioFormatFlagIsNonInterleaved) ? m->bytes_per_sample : chan * m->bytes_per_sample;
    streamFormat.mBytesPerFrame = (m->flags & kAudioFormatFlagIsNonInterleaved) ? m->bytes_per_sample : chan * m->bytes_per_sample;

    if (AudioUnitSetProperty( gAudioDevice.outputInstance, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamFormat, sizeof( streamFormat ) ))
    {
        tePrint( "Failed to set audio unit input property.\n" );
        return false;
    }

    if (AudioUnitSetProperty( gAudioDevice.outputInstance, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, callback, sizeof( AURenderCallbackStruct ) ) )
    {
        tePrint( "Unable to attach an IOProc to the selected audio unit.\n" );
        return false;
    }

    if (AudioUnitInitialize( gAudioDevice.outputInstance ))
    {
        tePrint( "Unable to initialize audio unit instance\n" );
        return false;
    }

    if (AudioOutputUnitStart( gAudioDevice.outputInstance ))
    {
        tePrint( "Unable to start audio unit.\n" );
        return false;
    }


    return true;
}

void PauseAudio( bool paused )
{
    if (paused)
    {
        AudioOutputUnitStop( gAudioDevice.outputInstance );
    }
    else if (AudioOutputUnitStart( gAudioDevice.outputInstance ))
    {
        tePrint( "Unable to restart audio unit after pausing.\n" );
        //close_audio();
    }
}

void InitAudio()
{
    AudioComponentDescription desc = {};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    gAudioDevice.outputComp = AudioComponentFindNext( nullptr, &desc );
    if (!gAudioDevice.outputComp)
    {
        tePrint( "Failed to open default audio device.\n" );
        return;
    }

    if (AudioComponentInstanceNew( gAudioDevice.outputComp, &gAudioDevice.outputInstance ))
    {
        tePrint( "Failed to open default audio device.\n" );
        return;
    }

    // set volume (value 0-100)
    {
        constexpr float VolumeRangeDb = 40; // decibels
        int value = 100;
        float factor = (value == 0) ? 0.0 : powf( 10, VolumeRangeDb * (value - 100) / 100 / 20 );

        AudioUnitSetParameter( gAudioDevice.outputInstance, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, factor, 0 );
    }
}

void LoadAudioWAV( const teFile& file, unsigned clipIndex )
{
    audioClipInternals[ clipIndex ].wavFile.data = (unsigned char*)teMalloc( file.size );
    audioClipInternals[ clipIndex ].wavFile.size = file.size;
    teMemcpy( audioClipInternals[ clipIndex ].wavFile.data, file.data, file.size );
    teMemcpy( audioClipInternals[ clipIndex ].wavFile.path, file.path, sizeof( file.path ) );

    audioClipInternals[ clipIndex ].data = LoadWAV( audioClipInternals[ clipIndex ].wavFile, audioClipInternals[ clipIndex ].sampleRate, audioClipInternals[ clipIndex ].channelCount, audioClipInternals[ clipIndex ].frameCount );
}

void PlayAudioClip( unsigned clipIndex )
{
    if (!audioClipInternals[ clipIndex ].data)
    {
        return;
    }

    AURenderCallbackStruct callback;
    callback.inputProc = tone;
    callback.inputProcRefCon = nullptr;

    gAudioDevice.wavPlaybackSample = 0;

    bool ok = OpenAudio( FMT_S16_LE, audioClipInternals[ clipIndex ].sampleRate, audioClipInternals[ clipIndex ].channelCount, &callback );
    if (!ok)
    {
        AudioComponentInstanceDispose( gAudioDevice.outputInstance );
        tePrint( "failed to open audio!\n" );
    }

    gAudioDevice.playingClipIndex = clipIndex;
}
