// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include "Clock.h"
#include "MagicNumbers.h"

using namespace NowSound;

// All values should work for this down to small multiples of a single audio frame, but one second is a
// nice round number, especially for hearing buffer clicks or other driver glitches.
const ContinuousDuration<Second> MagicNumbers::AudioFrameLengthSeconds{ (float)1 };

// exactly one beat per second for initial testing
const float MagicNumbers::InitialBeatsPerMinute{ 60 };

// 4/4 time
const int MagicNumbers::BeatsPerMeasure{ 4 };

// Stereo
const int MagicNumbers::AudioChannelCount{ 2 };

// 8 preallocated buffers
const int MagicNumbers::InitialAudioBufferCount{ 8 };

// Duration of each audio buffer in seconds
const int MagicNumbers::AudioBufferSizeInSeconds{ 1 };

// 1/10 sec seems fine for NowSound with TASCAM US2x2 :-P  -- this should probably be user-tunable or even autotunable...
const Duration<AudioSample> MagicNumbers::TrackLatencyCompensation{ 0 /*Clock::SampleRateHz / 8*/ };

// This could easily be huge but 1000 is fine for getting at least a second's worth of per-track history at audio rate.
const int MagicNumbers::DebugLogCapacity{ 1000 };

// 200 histogram values at 100Hz = two seconds of history, enough to follow transient crackling/breakup
// (due to losing foreground execution status, for example)
const int MagicNumbers::AudioQuantumHistogramCapacity{ 200 };
