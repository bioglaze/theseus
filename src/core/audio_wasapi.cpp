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
};

AudioDevice gAudioDevice;

WAVEFORMATEXTENSIBLE MakeAudioFormat( int channelCount, int sampleRate, int sampleSize )
{
    // translate format specification to WAVEFORMATEXTENSIBLE
    WAVEFORMATEXTENSIBLE result = {};
    result.dwChannelMask = 0;
    result.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    result.Samples.wValidBitsPerSample = sampleSize * 8;
    result.Format.nChannels = channelCount;
    result.Format.nSamplesPerSec = sampleRate;
    result.Format.wBitsPerSample = sampleSize * 8;
    result.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    result.Format.cbSize = sizeof( WAVEFORMATEXTENSIBLE );
    result.Format.nBlockAlign = channelCount * sampleSize;
    result.Format.nAvgBytesPerSec = channelCount * sampleSize * sampleRate;
    return result;
}

void InitAudio()
{
    REFERENCE_TIME engine;
    REFERENCE_TIME period;

    HRESULT hr = CoCreateInstance( __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof( IMMDeviceEnumerator ), reinterpret_cast<void**>( &gAudioDevice.enumerator ) );
    teAssert( SUCCEEDED( hr ) );
    hr = gAudioDevice.enumerator->GetDefaultAudioEndpoint( eRender, eMultimedia, &gAudioDevice.device );
    teAssert( SUCCEEDED( hr ) );
    hr = gAudioDevice.device->Activate( __uuidof( IAudioClient ), CLSCTX_ALL,
        nullptr, reinterpret_cast<void**>( &gAudioDevice.client ) );
    teAssert( SUCCEEDED( hr ) );

    // open exclusive mode event driven stream
    hr = gAudioDevice.client->GetDevicePeriod( &engine, &period );
    teAssert( SUCCEEDED( hr ) );
}
