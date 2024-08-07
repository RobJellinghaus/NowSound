// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include "Clock.h"
#include "MagicConstants.h"

using namespace NowSound;

// TODO: move all these magic numbers out of the library! They are tuning parameters to be configured by clients.

// If true, create audio graph with UseLowestLatency and RawMode.
const bool MagicConstants::UseLowestLatency = true;

// exactly one beat per second for initial testing
const float MagicConstants::InitialBeatsPerMinute{ 60 };

// 4/4 time
const int MagicConstants::BeatsPerMeasure{ 4 };

// Could be much larger but not really any reason to
const int MagicConstants::InitialAudioBufferCount{ 8 };

// 1 second of stereo float audio at 48Khz is only 384KB.  One second buffer ensures minimal fragmentation
// regardless of loop length.
const Duration<Second> MagicConstants::AudioBufferSizeInSeconds{ 1 };

// This could easily be huge but 1000 is fine for getting at least a second's worth of per-track history at audio rate.
const int MagicConstants::DebugLogCapacity{ 1000 };

// 200 histogram values at 100Hz = two seconds of history, enough to follow transient crackling/breakup
// (due to losing foreground execution status, for example)
const int MagicConstants::AudioQuantumHistogramCapacity{ 200 };

// Volume over the last half second works well enough in practice
const ContinuousDuration<Second> MagicConstants::RecentVolumeDuration{ (float)0.5 };

// For now, one-half beat is OK for ending a single-beat loop late.
const ContinuousDuration<Beat> MagicConstants::SingleTruncationBeats{ (float)0.5 };

// One beat is about right for ending a multi-beat loop late.
const ContinuousDuration<Beat> MagicConstants::MultiTruncationBeats{ (float)1 };
