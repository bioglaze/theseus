#include "audio.h"
#include <alsa/asoundlib.h>
#include "file.h"
#include "te_stdlib.h"

int16_t* LoadWAV( teFile& file, int& outSampleRate, int& outChannelCount, int& outFrameCount );

struct AudioDevice
{
    snd_pcm_t* device = nullptr;
    unsigned playingClipIndex = 0;
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

void InitAudio()
{
    int err = snd_pcm_open( &gAudioDevice.device, "default", SND_PCM_STREAM_PLAYBACK, 0 );

    if (err < 0)
    {
        tePrint( "Unable to open sound device: %s\n", snd_strerror( err ) );
        return;
    }

    snd_pcm_hw_params_t* hwParams;
    if ((err = snd_pcm_hw_params_malloc( &hwParams )) < 0)
    {
        tePrint( "Parameter allocation failed: %s\n", snd_strerror( err ) );
    }

    if ((err = snd_pcm_hw_params_any( gAudioDevice.device, hwParams ) ) < 0)
    {
        tePrint( "Parameter init failed: %s\n", snd_strerror( err ) );
    }

    unsigned resample = 1;
    err = snd_pcm_hw_params_set_rate_resample( gAudioDevice.device, hwParams, resample );
    if (err < 0)
    {
        tePrint( "Parameter resample failed: %s\n", snd_strerror( err ) );
    }

    if ((err = snd_pcm_hw_params_set_access( gAudioDevice.device, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
        tePrint( "Parameter access failed: %s\n", snd_strerror( err ) );
    }

    unsigned actualRate = 44100;
    if ((err = snd_pcm_hw_params_set_rate_near( gAudioDevice.device, hwParams, &actualRate, 0 ) ) < 0)
    {
        tePrint( "Parameter sample rate failed: %s\n", snd_strerror( err ) );        
    }

    if (actualRate < 44100)
    {
        tePrint( "Actual rate: %d\n", actualRate );
    }

    if ((err = snd_pcm_hw_params( gAudioDevice.device, hwParams )) < 0)
    {
        tePrint( "Parameter apply failed: %s\n", snd_strerror( err ) );
    }

    snd_pcm_uframes_t bufferSize;
    snd_pcm_hw_params_get_buffer_size( hwParams, &bufferSize );
    tePrint( "bufferSize: %lu\n", bufferSize );

    tePrint( "Significant bits for linear samples: %d\n", snd_pcm_hw_params_get_sbits( hwParams ) );
    snd_pcm_hw_params_free( hwParams );

    if ((err = snd_pcm_prepare( gAudioDevice.device )) < 0)
    {
        tePrint( "Prepare failed: %s\n", snd_strerror( err ) );
    }

    //tePrint( "Device initialized successfully, uninitializing now.\n" );
    //snd_pcm_close( gAudioDevice.device );
}

void LoadAudioWAV( const char* path, unsigned clipIndex )
{
    audioClipInternals[ clipIndex ].wavFile = teLoadFile( path );
    audioClipInternals[ clipIndex ].data = LoadWAV( audioClipInternals[ clipIndex ].wavFile, audioClipInternals[ clipIndex ].sampleRate, audioClipInternals[ clipIndex ].channelCount, audioClipInternals[ clipIndex ].frameCount );
}

void PlayAudioClip( unsigned clipIndex )
{ 
    gAudioDevice.playingClipIndex = clipIndex;
    
    int err = snd_pcm_set_params( gAudioDevice.device,
                                  SND_PCM_FORMAT_S16_LE,
                                  SND_PCM_ACCESS_RW_INTERLEAVED,
                                  1,
                                  audioClipInternals[ clipIndex ].sampleRate,
                                  1,
                                  500000 );

    int frames = snd_pcm_writei( gAudioDevice.device, audioClipInternals[ clipIndex ].data, audioClipInternals[ clipIndex ].frameCount );
    if (frames < 0)
    {
        frames = snd_pcm_recover( gAudioDevice.device, frames, 0 );
    }
    if (frames < 0)
    {
        tePrint("snd_pcm_writei failed: %s\n", snd_strerror( frames ));
        return;
    }
    if (frames > 0 && frames < (long)sizeof(audioClipInternals[ clipIndex ].frameCount))
    {
        tePrint( "Short write\n" );
    }
}
