// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "NowSoundTime.h"

// Constants that are assigned based on manual tuning.
// These are collected into one location to avoid their being scattered subtly around the code.
// Effectively, there should be no constant numeric values other than 0 or 1 anywhere in NowSoundLib
// except in MagicConstants.cpp.
namespace NowSound
{
    class MagicConstants
    {
    public:
		// Request the lowest latency?
		// All things being equal this is obviously what we want; however unfortunately Windows, and Windows
		// drivers for audio hardware, can be very unpredictable regarding performance of actual low latency
		// capture and playback.
		static const bool UseLowestLatency;

        // The initial tempo.
        static const float InitialBeatsPerMinute;

        // The number of beats in a measure. (e.g. 4 for 4/4 time)
        // TODO: make this no longer constant; fun with time signatures!
        static const int BeatsPerMeasure;

        // How many (one-second, for now) audio buffers do we initially want to allocate?
        // Not much downside to allocating many; stereo float 48Khz = only 384KB per one-sec buffer
        static const int InitialAudioBufferCount;

        // How many seconds long is each audio buffer?
        static const Duration<Second> AudioBufferSizeInSeconds;

        // The amount of time by which to "pre-record" already-heard audio at the start of a new track.
        static const ContinuousDuration<Second> PreRecordingDuration;

        // The number of strings to buffer in the per-track debug log.
        static const int DebugLogCapacity;

        // How many audio frames' duration will the per-track histogram follow?
        // The histogram helps detect spikes in the latency observed by the FrameInputNode_QuantumStarted method.
        static const int AudioQuantumHistogramCapacity;

		// Amount of time over which to measure volume.
		static const ContinuousDuration<Second> RecentVolumeDuration;
    };
}
