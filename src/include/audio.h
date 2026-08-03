#pragma once

struct teAudioClip
{
    unsigned index = 0;
};

teAudioClip teLoadAudioClip( const struct teFile& wavFile );
void tePlayAudioClip( teAudioClip clip );
void InitAudio();
