// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

#include <future>

#include "stdint.h"

#include "Check.h"

namespace NowSound
{
    // All external methods here are static and use C linkage, for P/Invokability.
    // AudioGraph object references are passed by integer ID; lifecycle is documented
    // per-API.
    // Callbacks are implemented by passing statically invokable callback hooks
    // together with callback IDs.
    extern "C"
    {
        // Static information about an Initialized audio graph.
		typedef struct NowSoundGraphInfo
		{
			// The sample rate of the graph in hertz.
			int32_t SampleRateHz;
			// The number of output channels
			int32_t ChannelCount;
			// The number of bits per sample.
			int32_t BitsPerSample;
			// The latency of the graph, in samples, as reported by the graph itself.
			int32_t LatencyInSamples;
			// The number of samples per audio graph quantum, also reported by the graph itself.
			int32_t SamplesPerQuantum;
			// The number of input devices.
			int32_t InputDeviceCount;
		} NowSoundGraphInfo;

		// Time information from a Created or Running graph.
		typedef struct NowSoundTimeInfo
		{
			// The number of samples elamsed since the audio graph started.
			int64_t TimeInSamples;
			// The exact current beat (including fractional part; truncate to get integral beat count).
			float ExactBeat;
			// The current BPM of the graph.
			float BeatsPerMinute;
			// The current position in the measure. (e.g. 4/4 time = this ranges from 0 to 3)
			float BeatInMeasure;
		} NowSoundTimeInfo;

		// Information about a created input.
		// TODO: make this be mono!  Mono inputs are more spatializable.
		typedef struct NowSoundInputInfo
		{
			// Volume for the two channels.  TODO: generalize to N channels.
			float Channel0Volume;
			float Channel1Volume;

			// The pan value; 0 = left, 1 = right, 0.5 = center.
			float Pan;
		} NowSoundInputInfo;

        // Information about a track's time in NowSound terms.
        typedef struct NowSoundTrackInfo
        {
            // The start time of the track, in samples from the beginning of this session.
            int64_t StartTimeInSamples;
            // The start time of the track, in beats.
            float StartTimeInBeats;
            // The duration of the track in audio samples.
            int64_t DurationInSamples;
            // The duration of the track in beats.
            int64_t DurationInBeats;
            // The duration of the track in exact seconds; DurationInSamples is this, rounded up to the nearest sample.
            float ExactDuration;
			// The clock time, relative to the start of the track.
			int64_t LocalClockTime;
			// The current beat of the track (e.g. a 12 beat track = this ranges from 0 to 11.999...).
            float LocalClockBeat;
			// The time at which the track last delivered samples (depends on current audio frame size).
			int64_t LastSampleTime;
			// The volume, averaged over the last N samples.
			float RecentVolume;
			// The minimum count of required samples over the last N seconds.
            float MinimumRequiredSamples;
            // The maximum count of required samples over the last N seconds.
            float MaximumRequiredSamples;
            // The average count of required samples over the last N seconds.
            float AverageRequiredSamples;
            // The minimum time since last quantum over the last N seconds.
            float MinimumTimeSinceLastQuantum;
            // The maximum time since last quantum over the last N seconds.
            float MaximumTimeSinceLastQuantum;
            // The average time since last quantum over the last N seconds.
            float AverageTimeSinceLastQuantum;
        } NowSoundTrackTimeInfo;

        // The states of a NowSound graph.
        // Note that since this is extern "C", this is not an enum class, so these identifiers have to begin with Track
        // to disambiguate them from the TrackState identifiers.
        enum NowSoundGraphState
        {
            // InitializeAsync() has not yet been called.
            GraphUninitialized,

            // Some error has occurred; GetLastError() will have details.
            GraphInError,

            // A gap in incoming audio data was detected; this should ideally never happen.
            // NOTYET: Discontinuity,

            // InitializeAsync() has completed; devices can now be queried.
            GraphInitialized,

            // CreateAudioGraphAsync() has completed; other methods can now be called.
            GraphCreated,

            // The audio graph has been started and is running.
            GraphRunning,

            // The audio graph has been stopped.
            // NOTYET: Stopped,
        };

        // The state of a particular IHolofunkAudioTrack.
        // Note that since this is extern "C", this is not an enum class, so these identifiers have to begin with Track
        // to disambiguate them from the GraphState identifiers.
        enum NowSoundTrackState
        {
            // This track is not initialized -- important for some state machine cases and for catching bugs
            // (also important that this be the default value)
            TrackUninitialized,

            // The track is being recorded and it is not known when it will finish.
            TrackRecording,

            // The track is finishing off its now-known recording time.
            TrackFinishRecording,

            // The track is playing back, looping.
            TrackLooping,
        };

        // The indices for audio inputs created by the app.
        /// Prevents confusing an audio input with some other int value.
        // 
        // The predefined values are really irrelevant; it can be cast to and from int as necessary.
        // But, used in parameters, the type helps with making the code self-documenting.
		// Note that zero is the default, undefined, invalid value, and that the actual inputs
		// are 1-based (to ensure zero is not mentioned).
        enum AudioInputId
        {
			AudioInputUndefined,
            AudioInput1,
            AudioInput2,
            AudioInput3,
            AudioInput4,
            AudioInput5,
            AudioInput6,
			AudioInput7,
			AudioInput8,
		};

        // The ID of a NowSound track; avoids issues with marshaling object references.
        // Note that 0 is the default, undefined, invalid value.
        enum TrackId
        {
            TrackIdUndefined
        };

		NowSoundGraphInfo CreateNowSoundGraphInfo(
			int32_t sampleRateHz,
			int32_t channelCount,
			int32_t bitsPerSample,
			int32_t latencyInSamples,
			int32_t samplesPerQuantum,
			int32_t inputDeviceCount);

		NowSoundTimeInfo CreateNowSoundTimeInfo(
			int64_t timeInSamples,
			float exactBeat,
			float beatsPerMinute,
			float beatInMeasure);

		NowSoundInputInfo CreateNowSoundInputInfo(
			float channel0Volume,
			float channel1Volume,
			float pan);

        NowSoundTrackInfo CreateNowSoundTrackInfo(
            int64_t startTimeInSamples,
            float startTimeInBeats,
            int64_t durationInSamples,
            int64_t durationInBeats,
            float exactDuration,
			int64_t localClockTime,
			float localClockBeat,
			int64_t lastSampleTime,
			float recentVolume,
            float minimumRequiredSamples,
			float maximumRequiredSamples,
			float averageRequiredSamples,
            float minimumTimeSinceLastQuantum,
			float maximumTimeSinceLastQuantum,
			float averageTimeSinceLastQuantum);
    }
}
