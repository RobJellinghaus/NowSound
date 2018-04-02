// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "Time.h"

// Constants that are assigned based on manual tuning.
// These are collected into one location to avoid their being scattered subtly around the code.
// Effectively, there should be no constant numeric values other than 0 or 1 anywhere in NowSoundLib
// except in MagicNumbers.cpp.
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

        // The amount of time by which to "pre-record" already-heard audio at the start of a new track.
        static const Duration<AudioSample> TrackLatencyCompensation;

        // The number of strings to buffer in the per-track debug log.
        static const int DebugLogCapacity;

        // How many audio frames' duration will the per-track histogram follow?
        // The histogram helps detect spikes in the latency observed by the FrameInputNode_QuantumStarted method.
        static const int AudioQuantumHistogramCapacity;
    };
}
