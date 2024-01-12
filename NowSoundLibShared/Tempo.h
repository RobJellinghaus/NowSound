#pragma once

// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include "stdint.h"

#include "Check.h"
#include "Clock.h"
#include "NowSoundTime.h"

namespace NowSound
{
    // Represents a time rate, based on a beats-per-minute value.
    // Some methods rely on Clock::Instance having been initialized.
    class Tempo
    {
    public:
        Tempo(float beatsPerMinute, int beatsPerMeasure, int sampleRateHz) :
            _beatsPerMinute{ beatsPerMinute },
            _beatsPerMeasure{ beatsPerMeasure },
            _sampleRateHz{ sampleRateHz }
        {}

    private:
        // The beats per minute value.
        const float _beatsPerMinute;

        // The beats per measure value.
        const int _beatsPerMeasure;

        // The only state we need from the Clock.
        const int _sampleRateHz;

    public:
        // The BPM of this Tempo.
        float BeatsPerMinute() const { return _beatsPerMinute; }

        // The beats per measure in this tempo.
        int BeatsPerMeasure() const { return _beatsPerMeasure; }

        // BeatsPerMinute in units of seconds.
        float BeatsPerSecond() const { return ((float)_beatsPerMinute) / (float)60; }

        // How many audio samples in one beat at this tempo?
        // This requires that Clock::Instance() exists.
        ContinuousDuration<AudioSample> BeatDuration() const
        {
            return _sampleRateHz / BeatsPerSecond();
        }

        // Approximately how many beats?
        ContinuousDuration<Beat> TimeToBeats(Time<AudioSample> time) const
        {
            return ContinuousDuration<Beat>((float)time.Value() / BeatDuration().Value());
        }

        // empirically seen some Beats values come too close to this
        const double Epsilon = 0.0001;

        // What fraction of a beat?
        ContinuousDuration<Beat> TimeToFractionalBeat(Time<AudioSample> time) const
        {
            float beatValue = TimeToBeats(time).Value();
            return ContinuousDuration<Beat>(beatValue - std::floor(beatValue));
        }
    };
}
