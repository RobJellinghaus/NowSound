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
        // Moment instances, which represent the time at the moment the clock was asked.  Moments in
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

        const double TicksPerSecond = 10 * 1000 * 1000;

        // Calculate the _beatDuration based on _beatsPerMinute.
        // TODO: this doesn't seem like a good way to do this -- why not do this at construction?
        void CalculateBeatDuration();

    public:
        // Advance this clock from an AudioGraph thread.
        void AdvanceFromAudioGraph(Duration<AudioSample> duration);

        // The beats per minute of this clock.
        // This is the most useful value for humans to control and see, and in fact pretty much all 
        // time in the system is derived from this.  This value can only currently be changed when
        // no tracks exist.</remarks>
        float BPM() const { return _beatsPerMinute; }
        void BPM(float value);

        int BytesPerSecond() const { return SampleRateHz * _inputChannelCount * sizeof(float); }

        double BeatsPerSecond() const { return ((double)_beatsPerMinute) / 60.0; }

        ContinuousDuration<AudioSample> BeatDuration() const { return _beatDuration; }

        int BeatsPerMeasure() { return _beatsPerMeasure; }
    };

    // Moments are immutable points in time, that can be converted to various
    // time measurements (timepoint-count, second, beat).
    struct Moment
    {
    private:
        Time<AudioSample> _time;
        Clock* _clock;

    public:
        Moment(Time<AudioSample> time, Clock* clock) : _time(time), _clock(clock)
        {
            Check(clock != nullptr); // must actually have a clock
        }

        Time<AudioSample> Time() { return _time; }

        // Approximately how many seconds?
        double Seconds() const { return ((double)_time.Value()) / _clock->SampleRateHz; }

        // Approximately how many beats?
        ContinuousDuration<Beat> Beats() const { return ContinuousDuration<Beat>((float)_time.Value() / _clock->BeatDuration().Value()); }

        // Exactly how many complete beats?
        // Beats are represented by ints as it's hard to justify longs; 2G beats = VERY LONG TRACK</remarks>
        Duration<Beat> CompleteBeats() const { return Duration<Beat>((int)Beats().Value()); }

        const double Epsilon = 0.0001; // empirically seen some Beats values come too close to this

        // What fraction of a beat?
        ContinuousDuration<Beat> FractionalBeat() const { return ContinuousDuration<Beat>(Beats().Value() - CompleteBeats().Value()); }
    };
}
