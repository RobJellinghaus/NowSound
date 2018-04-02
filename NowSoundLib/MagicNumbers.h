// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "Time.h"

// Constants that are assigned based on manual tuning.
// These are collected into one location to avoid their being scattered subtly around the code.
namespace NowSound
{
    class MagicNumbers
    {
    public:
        // The length of the audio frame used for copying data both on input and output.
        // Making this shorter potentially reduces latency or at least input variability.
        // Making this longer reduces the number of times frame input nodes require data,
        // reducing CPU usage and context switching.
        static const ContinuousDuration<Second> AudioFrameLengthSeconds;

        // The initial tempo.
        static const float InitialBeatsPerMinute;

        // The number of beats in a measure. (e.g. 4 for 4/4 time)
        // TODO: make this no longer constant; fun with time signatures!
        static const int BeatsPerMeasure;

        // The number of audio channels.  (e.g. 2 for stereo.)
        // TODO: make this no longer constant; support 7.1 systems!
        static const int AudioChannelCount;

        // How many (one-second, for now) audio buffers do we initially want to allocate?
        // Not much downside to allocating many; stereo float 48Khz = only 384KB per one-sec buffer
        static const int InitialAudioBufferCount;
    };
}
