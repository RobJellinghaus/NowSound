// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include "NowSoundLibTypes.h"

namespace NowSound
{
    NowSoundDeviceInfo CreateNowSoundDeviceInfo(LPWSTR id, LPWSTR name)
    {
        NowSoundDeviceInfo info;
        info.Id = id;
        info.Name = name;
        return info;
    }

    NowSoundGraphInfo CreateNowSoundGraphInfo(int32_t latencyInSamples, int32_t samplesPerQuantum)
    {
        NowSoundGraphInfo info;
        info.LatencyInSamples = latencyInSamples;
        info.SamplesPerQuantum = samplesPerQuantum;
        return info;
    }

    NowSoundTimeInfo CreateNowSoundTimeInfo(int64_t timeInSamples, float exactBeat, float beatsPerMinute, float beatInMeasure)
    {
        NowSoundTimeInfo info;
        info.TimeInSamples = timeInSamples;
        info.ExactBeat = exactBeat;
        info.BeatsPerMinute = beatsPerMinute;
        info.BeatInMeasure = beatInMeasure;
        return info;
    }

    NowSoundTrackTimeInfo CreateNowSoundTrackTimeInfo(
        int64_t startTimeInSamples,
        float startTimeInBeats,
        int64_t durationInSamples,
        int64_t durationInBeats,
        float exactDuration,
		int64_t localClockTime,
		float localClockBeat,
		int64_t lastSampleTime,
		float minimumRequiredSamples,
        float maximumRequiredSamples,
        float averageRequiredSamples,
        float minimumTimeSinceLastQuantum,
        float maximumTimeSinceLastQuantum,
        float averageTimeSinceLastQuantum)
    {
        NowSoundTrackTimeInfo info;
        info.StartTimeInSamples = startTimeInSamples;
        info.StartTimeInBeats = startTimeInBeats;
        info.DurationInSamples = durationInSamples;
        info.DurationInBeats = durationInBeats;
        info.ExactDuration = exactDuration;
		info.LocalClockTime = localClockTime;
		info.LocalClockBeat = localClockBeat;
		info.LastSampleTime = lastSampleTime;
		info.MinimumRequiredSamples = minimumRequiredSamples;
        info.MaximumRequiredSamples = maximumRequiredSamples;
        info.AverageRequiredSamples = averageRequiredSamples;
        info.MinimumTimeSinceLastQuantum = minimumTimeSinceLastQuantum;
        info.MaximumTimeSinceLastQuantum = maximumTimeSinceLastQuantum;
        info.AverageTimeSinceLastQuantum = averageTimeSinceLastQuantum;
        return info;
    }
}
