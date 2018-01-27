// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "Clock.h"

using namespace NowSound;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace Windows::Foundation;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Render;

static std::unique_ptr<NowSound::Clock> NowSound::Clock::s_instance;

void NowSound::Clock::Initialize(float beatsPerMinute, int beatsPerMeasure, int inputChannelCount)
{
    std::unique_ptr<Clock> clock = new Clock(beatsPerMinute, beatsPerMeasure, inputChannelCount);
    s_instance = std::move(clock);
}

NowSound::Clock::Clock(float beatsPerMinute, int beatsPerMeasure, int inputChannelCount)
{
    Check(s_instance == null); // No Clock yet

    _beatsPerMinute = beatsPerMinute;
    _beatsPerMeasure = beatsPerMeasure;
    _inputChannelCount = inputChannelCount;

    CalculateBeatDuration();
}

void NowSound::Clock::CalculateBeatDuration()
{
    _beatDuration = (ContinuousDuration<AudioSample>)(((float)SampleRateHz * 60f) / _beatsPerMinute);
}

// Advance this clock from an AudioGraph thread.
public void AdvanceFromAudioGraph(Duration<AudioSample> duration)
{
    _audioTime += duration;
}

// Moments are immutable points in time, that can be converted to various
// time measurements (timepoint-count, second, beat).
public struct Moment
{
    public readonly Time<AudioSample> Time;

    public readonly Clock Clock;

    internal Moment(Time<AudioSample> time, Clock clock)
    {
        Time = time;
        Clock = clock;
    }

    // Approximately how many seconds?
    public double Seconds{ get{ return ((double)Time) / Clock.SampleRateHz; } }

        // Exactly how many beats?
    public ContinuousDuration<Beat> Beats{ get{ return (ContinuousDuration<Beat>)((double)Time / (double)Clock.BeatDuration); } }

        // Exactly how many complete beats?
        // Beats are represented by ints as it's hard to justify longs; 2G beats = VERY LONG TRACK</remarks>
    public Duration<Beat> CompleteBeats{ get{ return (int)Beats; } }

    private const double Epsilon = 0.0001; // empirically seen some Beats values come too close to this

                                           // What fraction of a beat?
    public ContinuousDuration<Beat> FractionalBeat{ get{ return (ContinuousDuration<Beat>)((float)Beats - (float)CompleteBeats); } }

        public override string ToString()
    {
        return "Moment[" + Time + "]";
    }
}
}
