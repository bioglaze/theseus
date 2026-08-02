#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include "te_stdlib.h"
#include <math.h>

AudioComponent output_comp;
AudioComponentInstance outputInstance;

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
    int bits_per_sample;
    int bytes_per_sample;
    unsigned int flags;
};

static struct CoreAudioFormatDescriptionMap formatMap[] =
{
    { FMT_S16_LE, 16, sizeof( int16_t ), kAudioFormatFlagIsSignedInteger },
    { FMT_S16_BE, 16, sizeof( int16_t ), kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian },
    { FMT_S32_LE, 32, sizeof( int32_t ), kAudioFormatFlagIsSignedInteger },
    { FMT_S32_BE, 32, sizeof( int32_t ), kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian },
    { FMT_FLOAT,  32, sizeof( float ),   kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved },
};

static double gtheta = 0;

OSStatus tone( void* inRef, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData )
{
    const double amplitude = 0.25;

    // Get the tone parameters out of the static var
    // could be stored in inRef
    double theta = gtheta;
    double theta_increment = 2.0 * M_PI * 440 / 44100;

    // This is a mono tone generator so we only need the first buffer
    const int channel = 0;
    Float32* buffer = (Float32* )ioData->mBuffers[ channel ].mData;

    // Generate the samples
    for (UInt32 frame = 0; frame < inNumberFrames; ++frame) 
    {
        buffer[ frame ] = sin( theta ) * amplitude;

        theta += theta_increment;
        if (theta > 2.0 * M_PI)
        {
            theta -= 2.0 * M_PI;
        }
    }

    gtheta = theta;

    return noErr;
}

bool OpenAudio( enum format_type format, int rate, int chan, AURenderCallbackStruct* callback )
{
    struct CoreAudioFormatDescriptionMap* m = nullptr;

    for (unsigned i = 0; i < 5; ++i)
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

    if (AudioUnitInitialize( outputInstance ))
    {
        tePrint( "Unable to initialize audio unit instance\n" );
        return false;
    }

    AudioStreamBasicDescription streamFormat;
    streamFormat.mSampleRate = rate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = m->flags;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mChannelsPerFrame = chan;
    streamFormat.mBitsPerChannel = m->bits_per_sample;
    streamFormat.mBytesPerPacket = chan * m->bytes_per_sample;
    streamFormat.mBytesPerFrame = chan * m->bytes_per_sample;

    if (AudioUnitSetProperty( outputInstance, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamFormat, sizeof( streamFormat ) ))
    {
        tePrint( "Failed to set audio unit input property.\n" );
        return false;
    }

    if (AudioUnitSetProperty( outputInstance, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, callback, sizeof( AURenderCallbackStruct ) ) )
    {
        tePrint( "Unable to attach an IOProc to the selected audio unit.\n" );
        return false;
    }

    if (AudioOutputUnitStart( outputInstance ))
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
        AudioOutputUnitStop( outputInstance );
    }
    else if (AudioOutputUnitStart( outputInstance ))
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

    output_comp = AudioComponentFindNext( nullptr, &desc );
    if (!output_comp)
    {
        tePrint( "Failed to open default audio device.\n" );
        return;
    }

    if (AudioComponentInstanceNew( output_comp, &outputInstance ))
    {
        tePrint( "Failed to open default audio device.\n" );
        return;
    }

    AURenderCallbackStruct callback;
    callback.inputProc = tone;
    callback.inputProcRefCon = nullptr; 

    bool ok = OpenAudio( FMT_FLOAT, 44100, 1, &callback );
    if (!ok)
    {
        AudioComponentInstanceDispose( outputInstance );
        tePrint( "failed to open audio!\n" );
        return;
    }

    // set volume (value 0-100)
    {
        constexpr float VolumeRangeDb = 40; // decibels
        unsigned value = 100;
        float factor = (value == 0) ? 0.0 : powf( 10, VolumeRangeDb * (value - 100) / 100 / 20 );

        AudioUnitSetParameter( outputInstance, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, factor, 0 );
    }
}
