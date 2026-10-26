#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <stdint.h>
#include "file.h"
#include "te_stdlib.h"

int16_t* LoadWAV( teFile& file, int& outSampleRate, int& outChannelCount, int& outFrameCount );

static constexpr unsigned MaxAudioClipsInternal = 1000;
static constexpr unsigned MaxVoices = 32;

// A slot is owned by the caller of PlayAudioClip while it's Claimed and by the mixer thread while it's Active.
enum VoiceState : LONG
{
    VoiceFree = 0,
    VoiceClaimed = 1,
    VoiceActive = 2
};

struct Voice
{
    volatile LONG state = VoiceFree;
    unsigned clipIndex = 0;
    double position = 0;  // Playback position in source frames.
    double rateRatio = 1; // Source frames advanced per output frame.
};

struct AudioDevice
{
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioRenderClient* render = nullptr;
    HANDLE bufferEvent = nullptr;
    HANDLE thread = nullptr;
    volatile LONG running = 0;
    unsigned sampleRate = 0; // Device mix rate, clips are resampled into it.
    unsigned channelCount = 0;
    UINT32 bufferFrames = 0;
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

AudioClipInternal audioClipInternals[ MaxAudioClipsInternal ]; // Indexed by audio_common.cpp audioClipIndex

Voice gVoices[ MaxVoices ];

static LONG AtomicRead( volatile LONG& value )
{
    return InterlockedCompareExchange( &value, 0, 0 );
}

// Mixes a voice into an interleaved float buffer. Returns false once the clip has been played to the end.
static bool MixVoice( Voice& voice, float* dst, UINT32 frameCount )
{
    const AudioClipInternal& clip = audioClipInternals[ voice.clipIndex ];
    const unsigned srcFrames = unsigned( clip.frameCount );
    const unsigned dstChannels = gAudioDevice.channelCount;
    constexpr float Int16ToFloat = 1.0f / 32768.0f;

    for (UINT32 frame = 0; frame < frameCount; ++frame)
    {
        const unsigned srcFrame = unsigned( voice.position );

        if (srcFrame >= srcFrames)
        {
            return false;
        }

        // The last frame interpolates against itself so that the clip doesn't read past its data.
        const unsigned nextFrame = (srcFrame + 1 < srcFrames) ? srcFrame + 1 : srcFrame;
        const float t = float( voice.position - srcFrame );

        float left = 0;
        float right = 0;

        if (clip.channelCount == 2)
        {
            const float l0 = clip.data[ srcFrame * 2 + 0 ] * Int16ToFloat;
            const float r0 = clip.data[ srcFrame * 2 + 1 ] * Int16ToFloat;
            const float l1 = clip.data[ nextFrame * 2 + 0 ] * Int16ToFloat;
            const float r1 = clip.data[ nextFrame * 2 + 1 ] * Int16ToFloat;
            left = l0 + (l1 - l0) * t;
            right = r0 + (r1 - r0) * t;
        }
        else
        {
            const float s0 = clip.data[ srcFrame ] * Int16ToFloat;
            const float s1 = clip.data[ nextFrame ] * Int16ToFloat;
            left = s0 + (s1 - s0) * t;
            right = left;
        }

        if (dstChannels == 1)
        {
            dst[ frame ] += (left + right) * 0.5f;
        }
        else
        {
            // Surround layouts get the clip on the front pair, the other speakers stay silent.
            dst[ frame * dstChannels + 0 ] += left;
            dst[ frame * dstChannels + 1 ] += right;
        }

        voice.position += voice.rateRatio;
    }

    return true;
}

static void MixDeviceBuffer()
{
    UINT32 padding = 0;

    if (FAILED( gAudioDevice.client->GetCurrentPadding( &padding ) ) || padding >= gAudioDevice.bufferFrames)
    {
        return;
    }

    const UINT32 frameCount = gAudioDevice.bufferFrames - padding;

    BYTE* raw = nullptr;

    if (FAILED( gAudioDevice.render->GetBuffer( frameCount, &raw ) ))
    {
        return;
    }

    float* dst = reinterpret_cast<float*>(raw);
    const size_t sampleCount = size_t( frameCount ) * gAudioDevice.channelCount;
    teZero( reinterpret_cast<char*>(dst), sampleCount * sizeof( float ) );

    unsigned mixedVoices = 0;

    for (unsigned i = 0; i < MaxVoices; ++i)
    {
        if (AtomicRead( gVoices[ i ].state ) != VoiceActive)
        {
            continue;
        }

        const bool stillPlaying = MixVoice( gVoices[ i ], dst, frameCount );
        ++mixedVoices;

        if (!stillPlaying)
        {
            InterlockedExchange( &gVoices[ i ].state, VoiceFree );
        }
    }

    if (mixedVoices > 0)
    {
        for (size_t i = 0; i < sampleCount; ++i)
        {
            dst[ i ] = dst[ i ] < -1.0f ? -1.0f : (dst[ i ] > 1.0f ? 1.0f : dst[ i ]);
        }
    }

    gAudioDevice.render->ReleaseBuffer( frameCount, mixedVoices > 0 ? 0 : AUDCLNT_BUFFERFLAGS_SILENT );
}

static DWORD WINAPI AudioThread( LPVOID )
{
    CoInitializeEx( nullptr, COINIT_MULTITHREADED );

    while (AtomicRead( gAudioDevice.running ) != 0)
    {
        // The timeout is only here so that shutdown isn't blocked if the device stops signalling.
        if (WaitForSingleObject( gAudioDevice.bufferEvent, 200 ) == WAIT_OBJECT_0)
        {
            MixDeviceBuffer();
        }
    }

    CoUninitialize();
    return 0;
}

void InitAudio()
{
    HRESULT hr = CoInitializeEx( nullptr, COINIT_MULTITHREADED );

    if (FAILED( hr ) && hr != RPC_E_CHANGED_MODE)
    {
        tePrint( "InitAudio: CoInitializeEx failed, audio is disabled.\n" );
        return;
    }

    if (FAILED( CoCreateInstance( __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&gAudioDevice.enumerator) ) ))
    {
        tePrint( "InitAudio: couldn't create a device enumerator, audio is disabled.\n" );
        return;
    }

    if (FAILED( gAudioDevice.enumerator->GetDefaultAudioEndpoint( eRender, eMultimedia, &gAudioDevice.device ) ))
    {
        tePrint( "InitAudio: couldn't get the default audio endpoint, audio is disabled.\n" );
        return;
    }

    if (FAILED( gAudioDevice.device->Activate( __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(&gAudioDevice.client) ) ))
    {
        tePrint( "InitAudio: couldn't activate an audio client, audio is disabled.\n" );
        return;
    }

    WAVEFORMATEX* mixFormat = nullptr;

    if (FAILED( gAudioDevice.client->GetMixFormat( &mixFormat ) ))
    {
        tePrint( "InitAudio: couldn't get the device mix format, audio is disabled.\n" );
        return;
    }

    // WAVE_FORMAT_EXTENSIBLE keeps the real format tag in the first four bytes of the SubFormat GUID.
    const unsigned formatTag = (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        ? reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat)->SubFormat.Data1
        : mixFormat->wFormatTag;

    if (formatTag != WAVE_FORMAT_IEEE_FLOAT)
    {
        tePrint( "InitAudio: the device mix format isn't 32-bit float, audio is disabled.\n" );
        CoTaskMemFree( mixFormat );
        return;
    }

    gAudioDevice.sampleRate = mixFormat->nSamplesPerSec;
    gAudioDevice.channelCount = mixFormat->nChannels;

    REFERENCE_TIME defaultPeriod = 0;
    REFERENCE_TIME minPeriod = 0;
    gAudioDevice.client->GetDevicePeriod( &defaultPeriod, &minPeriod );

    // Shared mode requires a periodicity of 0. The mixer thread is woken by bufferEvent instead of polling.
    hr = gAudioDevice.client->Initialize( AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        defaultPeriod, 0, mixFormat, nullptr );
    CoTaskMemFree( mixFormat );

    if (FAILED( hr ))
    {
        tePrint( "InitAudio: couldn't initialize the audio client, audio is disabled.\n" );
        return;
    }

    gAudioDevice.bufferEvent = CreateEventW( nullptr, FALSE, FALSE, nullptr );

    if (!gAudioDevice.bufferEvent || FAILED( gAudioDevice.client->SetEventHandle( gAudioDevice.bufferEvent ) ))
    {
        tePrint( "InitAudio: couldn't set the buffer event, audio is disabled.\n" );
        return;
    }

    if (FAILED( gAudioDevice.client->GetService( __uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(&gAudioDevice.render) ) ))
    {
        tePrint( "InitAudio: couldn't get a render client, audio is disabled.\n" );
        return;
    }

    if (FAILED( gAudioDevice.client->GetBufferSize( &gAudioDevice.bufferFrames ) ))
    {
        tePrint( "InitAudio: couldn't query the buffer size, audio is disabled.\n" );
        return;
    }

    InterlockedExchange( &gAudioDevice.running, 1 );
    gAudioDevice.thread = CreateThread( nullptr, 0, AudioThread, nullptr, 0, nullptr );

    if (!gAudioDevice.thread)
    {
        InterlockedExchange( &gAudioDevice.running, 0 );
        tePrint( "InitAudio: couldn't create the mixer thread, audio is disabled.\n" );
        return;
    }

    SetThreadPriority( gAudioDevice.thread, THREAD_PRIORITY_TIME_CRITICAL );

    if (FAILED( gAudioDevice.client->Start() ))
    {
        InterlockedExchange( &gAudioDevice.running, 0 );
        tePrint( "InitAudio: couldn't start the audio client, audio is disabled.\n" );
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
    if (AtomicRead( gAudioDevice.running ) == 0)
    {
        return;
    }

    if (clipIndex == 0 || clipIndex >= MaxAudioClipsInternal)
    {
        tePrint( "PlayAudioClip got an invalid clip index %u!\n", clipIndex );
        return;
    }

    const AudioClipInternal& clip = audioClipInternals[ clipIndex ];

    if (!clip.data || clip.frameCount <= 0 || clip.sampleRate <= 0)
    {
        tePrint( "PlayAudioClip tried to play a clip that is not loaded: %s\n", clip.wavFile.path );
        return;
    }

    for (unsigned i = 0; i < MaxVoices; ++i)
    {
        if (InterlockedCompareExchange( &gVoices[ i ].state, VoiceClaimed, VoiceFree ) == VoiceFree)
        {
            gVoices[ i ].clipIndex = clipIndex;
            gVoices[ i ].position = 0;
            gVoices[ i ].rateRatio = double( clip.sampleRate ) / double( gAudioDevice.sampleRate );

            // Publishing the state last hands the slot over with the fields above already visible to the mixer thread.
            InterlockedExchange( &gVoices[ i ].state, VoiceActive );
            return;
        }
    }

    tePrint( "PlayAudioClip ran out of voices, %s was dropped.\n", clip.wavFile.path );
}
