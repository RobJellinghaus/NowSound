// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include "Clock.h"
#include "MagicNumbers.h"

using namespace NowSound;

// This is a balance between small (lower latency but more risk of crackling if AudioGraph is moody)
// and large (lower CPU, but more latency in some situations... at least I'm pretty sure... need to
// confirm this)
const ContinuousDuration<Second> MagicNumbers::AudioFrameLengthSeconds{ (float)0.1 };

// exactly one beat per second for initial testing
const float MagicNumbers::InitialBeatsPerMinute{ 60 };

// 4/4 time
const int MagicNumbers::BeatsPerMeasure{ 4 };

// Stereo
const int MagicNumbers::AudioChannelCount{ 2 };

// Could be much larger but not really any reason to
const int MagicNumbers::InitialAudioBufferCount{ 8 };

// 1 second is a reasonable length for each preallocated audio buffer; not too much fragmentation (not that it matters,
// each 1 sec buffer is only 384KB)
const Duration<Second> MagicNumbers::AudioBufferSizeInSeconds{ 1 };

// 1/5 sec seems fine for NowSound with TASCAM US2x2 :-P  -- this should probably be user-tunable or even autotunable...
const ContinuousDuration<Second> MagicNumbers::PreRecordingDuration{ (float)0.2 };

// This could easily be huge but 1000 is fine for getting at least a second's worth of per-track history at audio rate.
const int MagicNumbers::DebugLogCapacity{ 1000 };

// 200 histogram values at 100Hz = two seconds of history, enough to follow transient crackling/breakup
// (due to losing foreground execution status, for example)
const int MagicNumbers::AudioQuantumHistogramCapacity{ 200 };

const ContinuousDuration<Second> MagicNumbers::RecentVolumeDuration{ (float)0.1 };

const int MagicNumbers::InputDeviceCount{ 1 };