// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include "MagicNumbers.h"

using namespace NowSound;

// 100Hz input frame rate, to minimize latency and stress the system a bit
const ContinuousDuration<Second> MagicNumbers::AudioFrameLengthSeconds{ (float)0.01 };

// exactly one beat per second for initial testing
const float MagicNumbers::InitialBeatsPerMinute{ 60 };

// 4/4 time
const int MagicNumbers::BeatsPerMeasure{ 4 };

// Stereo
const int MagicNumbers::AudioChannelCount{ 2 };

// 128 secs preallocated buffers
const int MagicNumbers::InitialAudioBufferCount{ 128 };
