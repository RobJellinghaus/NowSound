#pragma once

// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include "stdint.h"

#include "Check.h"
#include "Time.h"

namespace NowSound
{
    // Tracks the current time (driven from the audio input if it exists, otherwise from the Unity deltaTime),
    // and converts it to seconds and beats.
    class Clock
    {
        // Since the audio thread is fundamentally driving the time, the current clock
        // reading is subject to change out from under the UI thread.  So the Clock hands out immutable
        // Time<AudioSample> instances, which represent the time at the moment the clock was asked.  Time<AudioSample>s in
        // turn can be converted to timepoint-counts,  seconds, and beats, consistently and without racing.

        static std::unique_ptr<Clock> s_instance;

    public:

        Clock(float beatsPerMinute, int beatsPerMeasure, int inputChannelCount);

        // Construct a Clock and initialize Clock.Instance.
        // This must be called exactly once per process; multiple calls will be contract failure (unless closely
        // overlapped in time in which case they will race).
        static void Initialize(float beatsPerMinute, int beatsPerMeasure, int inputChannelCount);

        // The singleton Clock used by the application.
        static Clock& Instance()
        {
            Check(s_instance != nullptr); // Clock must have been initialized
            return *(s_instance.get());
        }

        // The rate of sound measurements (individual sample data points) per second.
        // TODO: don't hardcode this!
        static const uint32_t SampleRateHz = 48000;

    private:
        // The current BPM of this Clock.
        float _beatsPerMinute;

        // The beats per MEASURE.  e.g. 3/4 time = 3 beats per measure.
        // TODO: make this actually mean something; it is only partly implemented right now.
        const int _beatsPerMeasure;

        // The number of samples since the beginning of Holofunk; incremented by the audio quantum.
        Time<AudioSample> _audioTime;

        // How many input channels are there?
        const int _inputChannelCount;

        // What is the floating-point duration of a beat, in samples?
        // This will be a non-integer value if the BPM does not exactly divide the sample rate.
        ContinuousDuration<AudioSample> _beatDuration;

        // Calculate the _beatDuration based on _beatsPerMinute.
        // TODO: this doesn't seem like a good way to do this -- why not do this at construction?
        void CalculateBeatDuration();

    public:
        // Number of 100ns units in one second; useful for constructing Windows::Foundation::TimeSpans.
        static const long TicksPerSecond;

        // Advance this clock from an AudioGraph thread.
        void AdvanceFromAudioGraph(Duration<AudioSample> duration);

        // The beats per minute of this clock.
        // This is the most useful value for humans to control and see, and in fact pretty much all 
        // time in the system is derived from this.  This value can only currently be changed when
        // no tracks exist.</remarks>
        float BeatsPerMinute() const { return _beatsPerMinute; }
        void BeatsPerMinute(float value);

        int BytesPerSecond() const { return SampleRateHz * _inputChannelCount * sizeof(float); }

        double BeatsPerSecond() const { return ((double)_beatsPerMinute) / 60.0; }

        ContinuousDuration<AudioSample> BeatDuration() const { return _beatDuration; }

        int BeatsPerMeasure() { return _beatsPerMeasure; }

        Time<AudioSample> Now() { return _audioTime; }

        // Approximately how many beats?
        ContinuousDuration<Beat> TimeToBeats(Time<AudioSample> time) const
        {
            return ContinuousDuration<Beat>(
                (float)time.Value() /
                Clock::Instance().BeatDuration().Value());
        }

        // Exactly how many complete beats?
        // Beats are represented by ints as it's hard to justify longs; 2G beats = VERY LONG TRACK</remarks>
        Duration<Beat> TimeToCompleteBeats(Time<AudioSample> time) const
        {
            return Duration<Beat>((int)TimeToBeats(time).Value());
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
