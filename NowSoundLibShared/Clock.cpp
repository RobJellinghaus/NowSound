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

std::unique_ptr<NowSound::Clock> NowSound::Clock::s_instance;

void NowSound::Clock::Initialize(float beatsPerMinute, int beatsPerMeasure, int inputChannelCount)
{
    std::unique_ptr<Clock> clock(new Clock(beatsPerMinute, beatsPerMeasure, inputChannelCount));
    Clock::s_instance = std::move(clock);
}

NowSound::Clock::Clock(float beatsPerMinute, int beatsPerMeasure, int inputChannelCount)
    : _beatsPerMinute(beatsPerMinute), _beatsPerMeasure(beatsPerMeasure), _inputChannelCount(inputChannelCount), _audioTime(0), _beatDuration(0)
{
    Check(s_instance == nullptr); // No Clock yet
    CalculateBeatDuration();
}

void NowSound::Clock::CalculateBeatDuration()
{
    _beatDuration = (ContinuousDuration<AudioSample>)(((float)SampleRateHz * 60) / _beatsPerMinute);
}

void NowSound::Clock::BeatsPerMinute(float value)
{
    _beatsPerMinute = value;
    CalculateBeatDuration();
}

const long NowSound::Clock::TicksPerSecond = 10 * 1000 * 1000;

void NowSound::Clock::AdvanceFromAudioGraph(Duration<AudioSample> duration)
{
    // TODO: += operator
    _audioTime = _audioTime + duration;
}
