#pragma once

// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include "stdint.h"

#include "Check.h"
#include "NowSoundTime.h"

namespace NowSound
{
    // Tracks the current time (driven from the audio input if it exists, otherwise from the Unity deltaTime),
    // and converts it to seconds and beats.
    class Clock
    {
    public:
        Clock(int sampleRateHz, int channelCount);

    private:
        // The sample rate.
        const int _sampleRateHz;
        
        // How many channels are there?
        const int _channelCount;

        // The number of samples since the beginning of Holofunk; incremented by the audio quantum.
        Time<AudioSample> _now;

    public:
        // Number of 100ns units in one second; useful for constructing Windows::Foundation::TimeSpans.
        static const long TicksPerSecond;

        // Advance this clock from an AudioGraph thread.
        void AdvanceFromAudioGraph(Duration<AudioSample> duration);

        int SampleRateHz() const { return _sampleRateHz; }

        int ChannelCount() const { return _channelCount; }

        int BytesPerSecond() const { return SampleRateHz() * _channelCount * sizeof(float); }

        Time<AudioSample> Now() { return _now; }

        // TODO: is this correct for rounding up? Right now it doesn't, so does this drop fractional samples?
        Duration<AudioSample> TimeToSamples(ContinuousDuration<Second> seconds) { return static_cast<int64_t>(SampleRateHz() * seconds.Value()); }

        // empirically seen some Beats values come too close to this
        const double Epsilon = 0.0001; 
     };
}
