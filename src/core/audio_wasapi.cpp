#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include "file.h"
#include "te_stdlib.h"

int16_t* LoadWAV( teFile& file, int& outSampleRate, int& outChannelCount, int& outFrameCount );

struct AudioDevice
{
    LARGE_INTEGER qpcCount;
    IMMDevice* device;
    IAudioClock* clock;
    IAudioClient* client;
    IAudioRenderClient* render;
    IMMDeviceEnumerator* enumerator;
    REFERENCE_TIME period;
    REFERENCE_TIME engine;
};

AudioDevice gAudioDevice;

struct AudioClipInternal
{
    int channelCount = 0;
    int sampleRate = 0;
    int frameCount = 0;
    teFile wavFile;
    int16_t* data = nullptr;
};

AudioClipInternal audioClipInternals[ 1000 ]; // Indexed by audio_common.cpp audioClipIndex

void CheckAudioHr( HRESULT hr )
{
    teAssert( SUCCEEDED( hr ) );
}

WAVEFORMATEXTENSIBLE MakeAudioFormat( int channelCount, int sampleRate, int sampleSize )
{
    WAVEFORMATEXTENSIBLE result = {};
    result.dwChannelMask = channelCount == 2 ? KSAUDIO_SPEAKER_STEREO : KSAUDIO_SPEAKER_MONO;
    result.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    result.Samples.wValidBitsPerSample = WORD( sampleSize * 8 );
    result.Format.nChannels = WORD( channelCount );
    result.Format.nSamplesPerSec = sampleRate;
    result.Format.wBitsPerSample = WORD( sampleSize * 8 );
    result.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    result.Format.cbSize = sizeof( WAVEFORMATEXTENSIBLE ) - sizeof( WAVEFORMATEX );
    result.Format.nBlockAlign = WORD( channelCount * sampleSize );
    result.Format.nAvgBytesPerSec = channelCount * sampleSize * sampleRate;
    return result;
}

void InitAudio()
{
    CheckAudioHr( CoInitializeEx( nullptr, COINIT_MULTITHREADED ) );
    CheckAudioHr( CoCreateInstance( __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof( IMMDeviceEnumerator ), reinterpret_cast<void**>( &gAudioDevice.enumerator ) ) );
    CheckAudioHr( gAudioDevice.enumerator->GetDefaultAudioEndpoint( eRender, eMultimedia, &gAudioDevice.device ) );
    CheckAudioHr( gAudioDevice.device->Activate( __uuidof( IAudioClient ), CLSCTX_ALL,
        nullptr, reinterpret_cast<void**>( &gAudioDevice.client ) ) );

    CheckAudioHr( gAudioDevice.client->GetDevicePeriod( &gAudioDevice.engine, &gAudioDevice.period ) );
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
    if (!gAudioDevice.device)
    {
        return;
    }

    if (!audioClipInternals[ clipIndex ].data)
    {
        tePrint( "PlayAudioClip tried to play a clip that is not loaded: %s\n", audioClipInternals[ clipIndex ].wavFile.path );
        return;
    }

    int sampleSize = 2;

    WAVEFORMATEXTENSIBLE format = MakeAudioFormat( audioClipInternals[ clipIndex ].channelCount, audioClipInternals[ clipIndex ].sampleRate, sampleSize );

    const int32_t frameSize = sampleSize * audioClipInternals[ clipIndex ].channelCount;

    // exclusive mode event driven must use 128-byte aligned buffers
    const int32_t alignmentRequirementBytes = 128;

    const int64_t millisPerSecond = 1000;
    const int64_t reftimesPerMilli = 10000;

    UINT32 buffer_frames = static_cast<uint32_t>(gAudioDevice.period / (float)reftimesPerMilli * audioClipInternals[ clipIndex ].sampleRate / (float)millisPerSecond);
    while ((buffer_frames * frameSize) % alignmentRequirementBytes != 0)
    {
        ++buffer_frames;
    }
    //REFERENCE_TIME bufferedPeriod = buffer_frames * millisPerSecond * reftimesPerMilli / sampleRate;

    int64_t refTimesPerSec = 10000000;

    REFERENCE_TIME requestedDuration = refTimesPerSec * 2;
    DWORD initStreamFlags = (AUDCLNT_STREAMFLAGS_RATEADJUST | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY);

    HRESULT hr = gAudioDevice.client->Initialize( AUDCLNT_SHAREMODE_SHARED, initStreamFlags,
        requestedDuration, 0, reinterpret_cast<WAVEFORMATEX*>(&format), nullptr );
    if (hr == E_INVALIDARG)
    {
        teAssert( !"E_INVALIDARG" );
    }
    else if (hr == AUDCLNT_E_NOT_INITIALIZED)
    {
        teAssert( !"AUDCLNT_E_NOT_INITIALIZED" );
    }
    else if (hr == AUDCLNT_E_DEVICE_IN_USE)
    {
        teAssert( !"AUDCLNT_E_DEVICE_IN_USE" );
    }
    else if (hr == AUDCLNT_E_INVALID_DEVICE_PERIOD)
    {
        teAssert( !"AUDCLNT_E_INVALID_DEVICE_PERIOD" );

    }
    else if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT)
    {
        teAssert( !"AUDCLNT_E_UNSUPPORTED_FORMAT" );
    }
    else if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
    {
        CheckAudioHr( gAudioDevice.client->GetBufferSize( &buffer_frames ) );

        requestedDuration = (REFERENCE_TIME)
            ((10000.0 * 1000 / audioClipInternals[ clipIndex ].sampleRate * buffer_frames) + 0.5);

        gAudioDevice.client->Release();

        CheckAudioHr( gAudioDevice.device->Activate( __uuidof(IAudioClient), CLSCTX_ALL,
            nullptr, reinterpret_cast<void**>(&gAudioDevice.client) ) );

        // Open the stream and associate it with an audio session.
        CheckAudioHr( gAudioDevice.client->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            requestedDuration,
            requestedDuration,
            reinterpret_cast<WAVEFORMATEX*>(&format),
            nullptr ) );
    }
    else
    {
        teAssert( SUCCEEDED( hr ) );
    }

    CheckAudioHr( gAudioDevice.client->GetService( __uuidof(IAudioClock), reinterpret_cast<void**>(&gAudioDevice.clock) ) );
    CheckAudioHr( gAudioDevice.client->GetService( __uuidof(IAudioRenderClient), reinterpret_cast<void**>(&gAudioDevice.render) ) );
    CheckAudioHr( gAudioDevice.client->Start() );

    uint32_t bufferSizeInFrames;
    CheckAudioHr( gAudioDevice.client->GetBufferSize( &bufferSizeInFrames ) );

    int wavPlaybackSample = 0;
    bool done = false;
    
    while (!done)
    {
        uint32_t bufferPadding;
        CheckAudioHr( gAudioDevice.client->GetCurrentPadding( &bufferPadding ) );

        int soundBufferLatency = bufferSizeInFrames / 50;
        int numFramesToWrite = soundBufferLatency - bufferPadding;

        int16_t* buffer;
        CheckAudioHr( gAudioDevice.render->GetBuffer( numFramesToWrite, (BYTE**)(&buffer) ) );

        for (int frameIndex = 0; frameIndex < numFramesToWrite; ++frameIndex)
        {
            *buffer++ = audioClipInternals[ clipIndex ].data[ wavPlaybackSample ]; // left
            if (audioClipInternals[ clipIndex ].channelCount == 2)
            {
                ++wavPlaybackSample;
                *buffer++ = audioClipInternals[ clipIndex ].data[ wavPlaybackSample ]; // right
            }

            ++wavPlaybackSample;
            if (wavPlaybackSample >= audioClipInternals[ clipIndex ].frameCount * audioClipInternals[ clipIndex ].channelCount)
            {
                done = true;
                gAudioDevice.client->Stop();
                break;
            }
        }
        CheckAudioHr( gAudioDevice.render->ReleaseBuffer( numFramesToWrite, 0 ) );
    }
}
