#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include "te_stdlib.h"

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

WAVEFORMATEXTENSIBLE MakeAudioFormat( int channelCount, int sampleRate, int sampleSize )
{
    WAVEFORMATEXTENSIBLE result = {};
    result.dwChannelMask = 0;
    result.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    result.Samples.wValidBitsPerSample = WORD( sampleSize * 8 );
    result.Format.nChannels = WORD( channelCount );
    result.Format.nSamplesPerSec = sampleRate;
    result.Format.wBitsPerSample = WORD( sampleSize * 8 );
    result.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    result.Format.cbSize = sizeof( WAVEFORMATEXTENSIBLE );
    result.Format.nBlockAlign = WORD( channelCount * sampleSize );
    result.Format.nAvgBytesPerSec = channelCount * sampleSize * sampleRate;
    return result;
}

void InitAudio()
{
    HRESULT hr = CoCreateInstance( __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof( IMMDeviceEnumerator ), reinterpret_cast<void**>( &gAudioDevice.enumerator ) );
    teAssert( SUCCEEDED( hr ) );
    hr = gAudioDevice.enumerator->GetDefaultAudioEndpoint( eRender, eMultimedia, &gAudioDevice.device );
    teAssert( SUCCEEDED( hr ) );
    hr = gAudioDevice.device->Activate( __uuidof( IAudioClient ), CLSCTX_ALL,
        nullptr, reinterpret_cast<void**>( &gAudioDevice.client ) );
    teAssert( SUCCEEDED( hr ) );

    hr = gAudioDevice.client->GetDevicePeriod( &gAudioDevice.engine, &gAudioDevice.period );
    teAssert( SUCCEEDED( hr ) );
}

void LoadAudioWAV( const char* path )
{
    int sampleRate = 0;
    int channelCount = 0;
    int frameCount = 0;
    int sampleSize = 2;
    teFile wavFile = teLoadFile( path );
    int16_t* data = LoadWAV( wavFile, sampleRate, channelCount, frameCount );

    WAVEFORMATEXTENSIBLE format = MakeAudioFormat( channelCount, sampleRate, sampleSize );

    const int32_t frameSize = sampleSize * channelCount;

    // exclusive mode event driven must use 128-byte aligned buffers
    const int32_t alignmentRequirementBytes = 128;

    const int64_t millisPerSecond = 1000;
    const int64_t reftimesPerMilli = 10000;

    UINT32 buffer_frames = static_cast<uint32_t>(gAudioDevice.period / reftimesPerMilli * sampleRate / millisPerSecond);
    while ((buffer_frames * frameSize) % alignmentRequirementBytes != 0)
    {
        ++buffer_frames;
    }
    REFERENCE_TIME bufferedPeriod = buffer_frames * millisPerSecond * reftimesPerMilli / sampleRate;

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
        hr = gAudioDevice.client->GetBufferSize( &buffer_frames );
        teAssert( SUCCEEDED( hr ) );

        requestedDuration = (REFERENCE_TIME)
            ((10000.0 * 1000 / sampleRate * buffer_frames) + 0.5);

        gAudioDevice.client->Release();

        hr = gAudioDevice.device->Activate( __uuidof(IAudioClient), CLSCTX_ALL,
            nullptr, reinterpret_cast<void**>(&gAudioDevice.client) );
        teAssert( SUCCEEDED( hr ) );

        // Open the stream and associate it with an audio session.
        hr = gAudioDevice.client->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            requestedDuration,
            requestedDuration,
            reinterpret_cast<WAVEFORMATEX*>(&format),
            nullptr );
        teAssert( SUCCEEDED( hr ) );
    }
    teAssert( SUCCEEDED( hr ) );

    hr = gAudioDevice.client->GetService( __uuidof(IAudioClock), reinterpret_cast<void**>(&gAudioDevice.clock) );
    teAssert( SUCCEEDED( hr ) );

    hr = gAudioDevice.client->GetService( __uuidof(IAudioRenderClient), reinterpret_cast<void**>(&gAudioDevice.render) );
    teAssert( SUCCEEDED( hr ) );

    hr = gAudioDevice.client->Start();
    teAssert( SUCCEEDED( hr ) );

    uint32_t bufferSizeInFrames;
    hr = gAudioDevice.client->GetBufferSize( &bufferSizeInFrames );
    teAssert( hr == S_OK );

    int wavPlaybackSample = 0;

    while (true)
    {
        uint32_t bufferPadding;
        hr = gAudioDevice.client->GetCurrentPadding( &bufferPadding );
        teAssert( hr == S_OK );

        uint32_t soundBufferLatency = bufferSizeInFrames / 50;
        uint32_t numFramesToWrite = soundBufferLatency - bufferPadding;

        int16_t* buffer;
        hr = gAudioDevice.render->GetBuffer( numFramesToWrite, (BYTE**)(&buffer) );
        teAssert( hr == S_OK );

        for (uint32_t frameIndex = 0; frameIndex < numFramesToWrite; ++frameIndex)
        {
            *buffer++ = data[ wavPlaybackSample ]; // left
            if (channelCount == 2)
            {
                *buffer++ = data[ wavPlaybackSample ]; // right
            }

            ++wavPlaybackSample;
            wavPlaybackSample %= frameCount;
        }
        hr = gAudioDevice.render->ReleaseBuffer( numFramesToWrite, 0 );
        teAssert( hr == S_OK );
    }
}