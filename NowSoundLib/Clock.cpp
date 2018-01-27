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
    std::unique_ptr<Clock> clock(new Clock(beatsPerMinute, beatsPerMeasure, inputChannelCount));
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

void NowSound::Clock::BPM(float value);
{
    _beatsPerMinute = value;
    CalculateBeatDuration();
}

void NowSound::Clock::AdvanceFromAudioGraph(Duration<AudioSample> duration)
{
    _audioTime += duration;
}
