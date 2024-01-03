// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"
#include "Clock.h"

using namespace NowSound;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace winrt::Windows::Foundation;

NowSound::Clock::Clock(int sampleRateHz, int channelCount)
    : _sampleRateHz(sampleRateHz),
    _channelCount(channelCount),
    _now(0)
{
}

const long NowSound::Clock::TicksPerSecond = 10 * 1000 * 1000;

void NowSound::Clock::AdvanceFromAudioGraph(Duration<AudioSample> duration)
{
    _now = _now + duration;
}
