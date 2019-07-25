// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include "NowSoundLibTypes.h"

namespace NowSound
{
	NowSoundGraphInfo CreateNowSoundGraphInfo(
		int32_t sampleRateHz,
		int32_t channelCount,
		int32_t bitsPerSample,
		int32_t latencyInSamples,
		int32_t samplesPerQuantum)
	{
		NowSoundGraphInfo info;
		info.SampleRateHz = sampleRateHz;
		info.ChannelCount = channelCount;
		info.BitsPerSample = bitsPerSample;
		info.LatencyInSamples = latencyInSamples;
		info.SamplesPerQuantum = samplesPerQuantum;
		return info;
	}

	NowSoundTimeInfo CreateNowSoundTimeInfo(
		// JUCETODO: int32_t audioInputCount,
		int64_t timeInSamples,
		float exactBeat,
		float beatsPerMinute,
		float beatInMeasure)
	{
		NowSoundTimeInfo info;
		info.TimeInSamples = timeInSamples;
		info.ExactBeat = exactBeat;
		info.BeatsPerMinute = beatsPerMinute;
		info.BeatInMeasure = beatInMeasure;
		return info;
	}

	NowSoundSpatialParameters CreateNowSoundInputInfo(
		float volume,
		float pan)
	{
		NowSoundSpatialParameters info;
		info.Volume = volume;
		info.Pan = pan;
		return info;
	}

    NowSoundSignalInfo CreateNowSoundSignalInfo(
        float min,
        float max,
        float avg)
    {
        NowSoundSignalInfo info;
        info.Min = min;
        info.Max = max;
        info.Avg = avg;
        return info;
    }

    NowSoundTrackInfo CreateNowSoundTrackInfo(
        bool isTrackLooping,
        int64_t startTimeInSamples,
        float startTimeInBeats,
        int64_t durationInSamples,
        int64_t durationInBeats,
        float exactDuration,
		int64_t localClockTime,
		float localClockBeat,
		int64_t lastSampleTime,
		float pan)
    {
        NowSoundTrackInfo info;
        info.IsTrackLooping = isTrackLooping ? 1 : 0;
        info.StartTimeInSamples = startTimeInSamples;
        info.StartTimeInBeats = startTimeInBeats;
        info.DurationInSamples = durationInSamples;
        info.DurationInBeats = durationInBeats;
        info.ExactDuration = exactDuration;
		info.LocalClockTime = localClockTime;
		info.LocalClockBeat = localClockBeat;
		info.LastSampleTime = lastSampleTime;
		info.Pan = pan;
        return info;
    }
}
