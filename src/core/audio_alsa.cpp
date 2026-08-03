#include <alsa/asoundlib.h>
#include "te_stdlib.h"

void InitAudio()
{
    snd_pcm_t* soundDevice;
    int err = snd_pcm_open( &soundDevice, "default", SND_PCM_STREAM_PLAYBACK, 0 );

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

    if ((err = snd_pcm_hw_params_any( soundDevice, hwParams ) ) < 0)
    {
        tePrint( "Parameter init failed: %s\n", snd_strerror( err ) );
    }

    unsigned resample = 1;
    err = snd_pcm_hw_params_set_rate_resample( soundDevice, hwParams, resample );
    if (err < 0)
    {
        tePrint( "Parameter resample failed: %s\n", snd_strerror( err ) );
    }

    if ((err = snd_pcm_hw_params_set_access( soundDevice, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
        tePrint( "Parameter access failed: %s\n", snd_strerror( err ) );
    }

    unsigned actualRate = 44100;
    if ((err = snd_pcm_hw_params_set_rate_near( soundDevice, hwParams, &actualRate, 0 ) ) < 0)
    {
        tePrint( "Parameter sample rate failed: %s\n", snd_strerror( err ) );        
    }

    if (actualRate < 44100)
    {
        tePrint( "Actual rate: %d\n", actualRate );
    }

    if ((err = snd_pcm_hw_params( soundDevice, hwParams)) < 0)
    {
        tePrint( "Parameter apply failed: %s\n", snd_strerror( err ) );
    }

    snd_pcm_uframes_t bufferSize;
    snd_pcm_hw_params_get_buffer_size( hwParams, &bufferSize );
    tePrint( "bufferSize: %lu\n", bufferSize );

    tePrint( "Significant bits for linear samples: %d\n", snd_pcm_hw_params_get_sbits( hwParams ) );
    snd_pcm_hw_params_free( hwParams );

    if ((err = snd_pcm_prepare( soundDevice )) < 0)
    {
        tePrint( "Prepare failed: %s\n", snd_strerror( err ) );
    }

    tePrint( "Device initialized successfully, uninitializing now.\n" );
    snd_pcm_close( soundDevice );
}
