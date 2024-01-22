// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"

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
        // Some NowSound use cases (e.g. running under Unity with the Visual Studio Tools For Unity debugger)
        // don't allow us to see any diagnostic output from NowSoundLib at all.  We need an actual API for fetching
        // diagnostic messages.  This struct represents the indices of the first and last log messages that currently
        // exist.
        typedef struct NowSoundLogInfo
        {
            // the number of log messages
            int32_t LogMessageCount;
        } NowSoundLogInfo;
        
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
            // JUCETODO: consider reintroducing input device selection.
            // int32_t InputDeviceCount;
        } NowSoundGraphInfo;

        // Time information from a Created or Running graph.
        typedef struct NowSoundTimeInfo
        {
            // The number of samples elapsed since the audio graph started.
            // NOTE: THIS IS ONLY FOR DEBUGGING! Once upon a time this was tracked in loops,
            // but it turns out absolute time is a terrible concept that brings only rigidity and
            // lack of expression.
            int64_t TimeInSamples;
            // The exact current beat (including fractional part; truncate to get integral beat count),
            // according to the graph's current tempo.
            // TODO: technically this should be player-specific....
            float ExactBeat;
            // The current BPM of the graph.
            // TODO: technically this should be player-specific....
            float BeatsPerMinute;
            // The number of beats per measure (time signature).
            // TODO: technically this should be player-specific....
            int BeatsPerMeasure;
            // The current position in the measure. (e.g. 4/4 time = this ranges from 0 to 3)
            // TODO: technically this should be player-specific....
            float BeatInMeasure;
        } NowSoundTimeInfo;

        // Information about a created input; currently only mono inputs are supported.
        // (Stereo inputs can be represented as a pair of mono inputs.)
        typedef struct NowSoundSpatialParameters
        {
            // Volume for the input signal.
            float Volume;

            // The pan value; 0 = left, 1 = right, 0.5 = center.
            float Pan;
        } NowSoundInputInfo;

        // Information about the min/max/average value of a signal (in raw float terms --
        // no RMS or decibel interpolation is performed).
        typedef struct NowSoundSignalInfo
        {
            float Min;
            float Max;
            float Avg;
        } NowSoundSignalInfo;

        // Information about a track's time in NowSound terms.
        // Note that "exact" means "with floating point precision." Tracks have a duration
        // measured in floating point samples, since a BPM tempo that is a prime number will
        // have a duration that is a fractional number of samples.
        typedef struct NowSoundTrackInfo
        {
            // Is this track looping? If not, it is still recording. We use a wasteful int32_t to avoid
            // packing issues.
            int64_t IsTrackLooping;
            // The duration of the track in beats; always an integer number.
            // TODO: implement non-quantized loops BECAUSE WHY NOT
            int64_t DurationInBeats;
            // The exact duration of the track in samples.
            float ExactDurationInSamples;
            // The track's exact time in samples, relative to the start of the track.
            float ExactTrackTimeInSamples;
            // The current beat of the track (e.g. a 12 beat track = this ranges from 0 to 11.999...).
            float ExactTrackBeat;
            // The panning value of this track; from 0 (left) to 1 (right).
            float Pan;
            // The current volume of this track; from 0 to 1.
            float Volume;
            // The tempo of this track in beats per minute.
            float BeatsPerMinute;
            // The number of beats per measure (e.g. time signature).
            int64_t BeatsPerMeasure;
        } NowSoundTrackInfo;

        // The states of a NowSound graph.
        // Note that since this is extern "C", this is not an enum class, so these identifiers have to begin with Graph
        // to disambiguate them from the TrackState identifiers.
        enum NowSoundGraphState
        {
            // Initial condition before Initialize() is called.
            GraphUninitialized,

            // Some error has occurred; GetLastError() will have details.
            GraphInError,

            // The audio graph has been initialized and is running.
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
        // Prevents confusing an audio input with some other int value.
        //
        // Note that audio inputs are always mono (at present, and perhaps indefinitely).
        // 
        // The predefined values are really irrelevant; it can be cast to and from int as necessary.
        // But, used in parameters, the type helps with making the code self-documenting.
        // Note that zero is the default, undefined, invalid value, and that the actual inputs
        // are 1-based (to ensure zero is not mentioned).
        enum AudioInputId
        {
            AudioInputUndefined = 0,
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
        // Note that 0 is the default, undefined, invalid value, to catch interop errors more easily.
        enum TrackId
        {
            TrackIdUndefined = 0
        };

        // The ID of a sound effects plugin.
        // Note that 0 is the default, undefined, invalid value, to catch interop errors more easily.
        enum PluginId
        {
            PluginIdUndefined = 0
        };

        // The ID of a sound effects plugin's program.
        // Note that 0 is the default, undefined, invalid value, to catch interop errors more easily.
        enum ProgramId
        {
            ProgramIdUndefined = 0
        };

        // The one-based index of an instantiated plugin.  (Zero is not a valid value, for catching P/Invoke bugs.)
        // Note that this is *not* a persistent identifier; it is the index of the plugin in the vector of plugins
        // associated with the given input or track.
        // TODO: revisit this if a persistent ID turns out to be more usable in the API.
        enum PluginInstanceIndex
        {
            PluginInstanceIndexUndefined = 0
        };

        // The state of an instantiated plugin; eventually will include parameter settings.
        typedef struct NowSoundPluginInstanceInfo
        {
            PluginId NowSoundPluginId;
            ProgramId NowSoundProgramId;
            int32_t DryWet_0_100;
        } NowSoundPluginInstanceInfo;

        NowSoundGraphInfo CreateNowSoundGraphInfo(
            int32_t sampleRateHz,
            int32_t channelCount,
            int32_t bitsPerSample,
            int32_t latencyInSamples,
            int32_t samplesPerQuantum
            // JUCETODO: , int32_t inputDeviceCount
            );

        NowSoundTimeInfo CreateNowSoundTimeInfo(
            //JUCETODO: int32_t audioInputCount,
            int64_t timeInSamples,
            float exactBeat,
            float beatsPerMinute,
            int beatsPerMeasure,
            float beatInMeasure);

        NowSoundSpatialParameters CreateNowSoundInputInfo(
            float volume,
            float pan);

        NowSoundSignalInfo CreateNowSoundSignalInfo(
            float min,
            float max,
            float avg);

        NowSoundTrackInfo CreateNowSoundTrackInfo(
            bool isTrackLooping,
            int64_t durationInBeats,
            float exactDurationInSamples,
            float exactTrackTimeInSamples,
            float exactTrackBeat,
            float pan,
            float volume,
            float beatsPerMinute,
            int64_t beatsPerMeasure);

        NowSoundPluginInstanceInfo CreateNowSoundPluginInstanceInfo(
            PluginId pluginId,
            ProgramId programId,
            int32_t dryWet_0_100);
    }
}
