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
		int32_t samplesPerQuantum
		// JUCETODO: , int32_t inputDeviceCount
		)
	{
		NowSoundGraphInfo info;
		info.SampleRateHz = sampleRateHz;
		info.ChannelCount = channelCount;
		info.BitsPerSample = bitsPerSample;
		info.LatencyInSamples = latencyInSamples;
		info.SamplesPerQuantum = samplesPerQuantum;
		// JUCETODO: info.InputDeviceCount = inputDeviceCount;
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
		// info.AudioInputCount = audioInputCount;
		info.TimeInSamples = timeInSamples;
		info.ExactBeat = exactBeat;
		info.BeatsPerMinute = beatsPerMinute;
		info.BeatInMeasure = beatInMeasure;
		return info;
	}

	NowSoundInputInfo CreateNowSoundInputInfo(
		float volume,
		float pan)
	{
		NowSoundInputInfo info;
		info.Volume = volume;
		info.Pan = pan;
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
		float recentVolume,
		float pan,
		float minimumRequiredSamples,
        float maximumRequiredSamples,
        float averageRequiredSamples,
        float minimumTimeSinceLastQuantum,
        float maximumTimeSinceLastQuantum,
        float averageTimeSinceLastQuantum)
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
		info.Volume = recentVolume;
		info.Pan = pan;
		info.MinimumRequiredSamples = minimumRequiredSamples;
        info.MaximumRequiredSamples = maximumRequiredSamples;
        info.AverageRequiredSamples = averageRequiredSamples;
        info.MinimumTimeSinceLastQuantum = minimumTimeSinceLastQuantum;
        info.MaximumTimeSinceLastQuantum = maximumTimeSinceLastQuantum;
        info.AverageTimeSinceLastQuantum = averageTimeSinceLastQuantum;
        return info;
    }
}
