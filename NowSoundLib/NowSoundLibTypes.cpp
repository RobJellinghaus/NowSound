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
        int beatsPerMeasure,
        float beatInMeasure)
    {
        NowSoundTimeInfo info;
        info.TimeInSamples = timeInSamples;
        info.ExactBeat = exactBeat;
        info.BeatsPerMinute = beatsPerMinute;
        info.BeatsPerMeasure = beatsPerMeasure;
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
        bool isPlaybackBackwards,
        int64_t durationInBeats,
        float exactDurationInSamples,
        float exactTrackTimeInSamples,
        float exactTrackBeat,
        float pan,
        float volume,
        float beatsPerMinute,
        int64_t beatsPerMeasure)
    {
        NowSoundTrackInfo info;
        info.IsTrackLooping = isTrackLooping ? 1 : 0;
        info.IsPlaybackBackwards = isPlaybackBackwards ? 1 : 0;
        info.DurationInBeats = durationInBeats;
        info.ExactDurationInSamples = exactDurationInSamples;
        info.ExactTrackTimeInSamples = exactTrackTimeInSamples;
        info.ExactTrackBeat = exactTrackBeat;
        info.Pan = pan;
        info.Volume = volume;
        info.BeatsPerMinute = beatsPerMinute;
        // definitely not exceeding 2^31 lol
        info.BeatsPerMeasure = static_cast<int>(beatsPerMeasure);
        return info;
    }

    NowSoundPluginInstanceInfo CreateNowSoundPluginInstanceInfo(
        PluginId pluginId,
        ProgramId programId,
        int32_t dryWet_0_100)
    {
        NowSoundPluginInstanceInfo info;
        info.NowSoundPluginId = pluginId;
        info.NowSoundProgramId = programId;
        info.DryWet_0_100 = dryWet_0_100;
        return info;
    }
}
