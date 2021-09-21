﻿// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

//using Holofunk.Core;
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

// P/Invoke wrapper types and classes for NowSoundLib.
namespace NowSoundLib
{
    public struct NowSoundLogInfo
    {
        public Int32 LogMessageCount;
    }

    // Information about an audio graph.
    // This marshalable struct corresponds to the C++ P/Invokable type.
    // Since this has no fields that have particular units (e.g. no durations or times),
    // we just make the marshalable struct public in this case.
    public struct NowSoundGraphInfo
    {
        public Int32 SampleRate;
        public Int32 ChannelCount;
        public Int32 BitsPerSample;
        public Int32 LatencyInSamples;
        public Int32 SamplesPerQuantum;
    }

    // Information about timing in a created or running graph.
    // This marshalable struct maps to the C++ P/Invokable type.
    internal struct NowSoundTimeInfo
    {
        public Int64 TimeInSamples;
        public float ExactBeat;
        public float BeatsPerMinute;
        public float BeatInMeasure;
    };

    // Information about an input in a created or running graph; all inputs are mono
    // (and can be panned at will).
    public struct NowSoundInputInfo
    {
        public float Volume;
        public float Pan;
    }

    // Information about a signal, in terms of the raw float signal values (no RMS or
    // decibel conversion is performed).
    public struct NowSoundSignalInfo
    {
        public float Min;
        public float Max;
        public float Avg;
    }

    // Information about the current graph time in NowSound terms.
    public struct TimeInfo
    {
        // The number of samples elapsed since the audio graph started.
        public readonly Time<AudioSample> TimeInSamples;
        // The exact current beat (including fractional part; truncate to get integral beat count).
        public readonly ContinuousDuration<Beat> ExactBeat;
        // The current BPM of the graph.
        public readonly float BeatsPerMinute;
        // The current position in the measure. (e.g. 4/4 time = this ranges from 0 to 3)
        public readonly float BeatInMeasure;

        internal TimeInfo(NowSoundTimeInfo pinvokeTimeInfo)
        {
            TimeInSamples = pinvokeTimeInfo.TimeInSamples;
            ExactBeat = pinvokeTimeInfo.ExactBeat;
            BeatsPerMinute = pinvokeTimeInfo.BeatsPerMinute;
            BeatInMeasure = pinvokeTimeInfo.BeatInMeasure;
        }

        internal TimeInfo(
            float beatsInMeasure,
            float beatsPerMinute,
            ContinuousDuration<Beat> exactBeat,
            Time<AudioSample> timeInSamples)
        {
            BeatInMeasure = beatsInMeasure;
            BeatsPerMinute = beatsPerMinute;
            ExactBeat = exactBeat;
            TimeInSamples = timeInSamples;
        }
    };

    // Information about a track's time in NowSound terms.
    // This marshalable struct maps to the C++ P/Invokable type.
    // TODO: maybe someday: look at custom marshalers to avoid the explicit (no-op) copy.
    internal struct NowSoundTrackInfo
    {
        // Only need one bit for this, but to avoid padding weirdness (observed in practice), we make this an int64.
        internal Int64 IsTrackLooping;
        internal Int64 StartTimeInSamples;
        internal float StartTimeInBeats;
        internal Int64 DurationInSamples;
        internal Int64 DurationInBeats;
        internal float ExactDuration;
        internal Int64 LocalClockTime;
        internal float LocalClockBeat;
        internal Int64 LastSampleTime;
        internal float Pan;
    };

    // Information about a track's time in NowSound terms.
    public struct TrackInfo
    {
        // Is the track currently looping? If not, it's still recording.
        public readonly bool IsTrackLooping;
        // The start time of the track in audio samples from the beginning of the session.
        public readonly Time<AudioSample> StartTime;
        // The start time of the track in (fractional) beats. (TODO?: add ContinuousTime<TTime>)
        public readonly ContinuousDuration<Beat> StartTimeInBeats;
        // The duration of the track in audio samples.
        public readonly Duration<AudioSample> Duration;
        // The duration of the track in (discrete) beats. (TODO?: allow non-beat-alignment)
        public readonly Duration<Beat> DurationInBeats;
        // The duration of the track in exact seconds; DurationInSamples is this, rounded up to the nearest sample.
        public readonly ContinuousDuration<Second> ExactDuration;
        // The local clock time (relative to the start of the current track); expressed as a duration.
        public readonly Duration<AudioSample> LocalClockTime;
        // The local clock time, in terms of beats.
        public readonly ContinuousDuration<Beat> LocalClockBeat;
        // The time at which the track last delivered samples.
        public readonly Time<AudioSample> LastSampleTime;
        // The current panning (0 = left, 1 = right).
        public readonly float Pan;

        internal TrackInfo(NowSoundTrackInfo pinvokeTrackInfo)
        {
            IsTrackLooping = pinvokeTrackInfo.IsTrackLooping > 0;
            StartTime = pinvokeTrackInfo.StartTimeInSamples;
            StartTimeInBeats = pinvokeTrackInfo.StartTimeInBeats;
            Duration = pinvokeTrackInfo.DurationInSamples;
            DurationInBeats = pinvokeTrackInfo.DurationInBeats;
            ExactDuration = pinvokeTrackInfo.ExactDuration;
            LocalClockTime = pinvokeTrackInfo.LocalClockTime;
            LocalClockBeat = pinvokeTrackInfo.LocalClockBeat;
            LastSampleTime = pinvokeTrackInfo.LastSampleTime;
            Pan = pinvokeTrackInfo.Pan;
        }

        public TrackInfo(
            Duration<AudioSample> duration,
            Duration<Beat> durationInBeats,
            ContinuousDuration<Second> exactDuration,
            bool isTrackLooping,
            Time<AudioSample> lastSampleTime,
            ContinuousDuration<Beat> localClockBeat,
            Duration<AudioSample> localClockTime,
            float pan,
            Time<AudioSample> startTime,
            ContinuousDuration<Beat> startTimeInBeats
            )
        {
            Duration = duration;
            DurationInBeats = durationInBeats;
            ExactDuration = exactDuration;
            IsTrackLooping = isTrackLooping;
            LastSampleTime = lastSampleTime;
            LocalClockBeat = localClockBeat;
            LocalClockTime = localClockTime;
            Pan = pan;
            StartTime = startTime;
            StartTimeInBeats = startTimeInBeats;
        }
    };

    // The states of a NowSound graph.
    // Note that since this is extern "C", this is not an enum class, so these identifiers have to begin with Track
    // to disambiguate them from the TrackState identifiers.
    public enum NowSoundGraphState
    {
        // InitializeAsync() has not yet been called.
        GraphUninitialized,

        // Some error has occurred; GetLastError() will have details.
        GraphInError,

        // The audio graph has been started and is running.
        GraphRunning,
    };

    // The state of a particular IHolofunkAudioTrack.
    // Note that since this is extern "C", this is not an enum class, so these identifiers have to begin with Track
    // to disambiguate them from the GraphState identifiers.
    public enum NowSoundTrackState
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

    // The audio inputs known to the app.
    /// Prevents confusing an audio input with some other int value.
    // 
    // The predefined values are really irrelevant; it can be cast to and from int as necessary.
    // But, used in parameters, the type helps with making the code self-documenting.
    public enum AudioInputId
    {
        AudioInputUndefined,
        AudioInput1,
        AudioInput2,
        AudioInput3,
        AudioInput4,
        AudioInput5,
        AudioInput6,
        AudioInput7,
    };

    /// <summary>
    /// "Typedef" (C# style) for track IDs.
    /// Default value (0) is not used.
    /// </summary>
    public enum TrackId
    {
        Undefined = 0
    }

    /// <summary>
    /// 1-based ID for sound effect plugin.
    /// </summary>
    public enum PluginId
    {
        Undefined = 0
    }

    /// <summary>
    /// 1-based ID for sound effect plugin's program (not globally unique; specific to plugin).
    /// </summary>
    public enum ProgramId
    {
        Undefined = 0
    }

    /// <summary>
    /// 1-based index of a plugin in the vector of plugins associated with the input or track.
    /// </summary>
    /// <remarks>
    /// TODO: determine whether this would be more usable if it was a stable ID rather than an index
    /// which could change depending on number of plugins instantiated.
    /// TODO: determine whether we really also want a query API to return the state of the instantiated
    /// plugins.
    /// </remarks>
    public enum PluginInstanceIndex
    {
        Undefined = 0
    }

    public struct PluginInstanceInfo
    {
        public readonly PluginId NowSoundPluginId;
        public readonly ProgramId NowSoundProgramId;
        public readonly Int32 DryWet_0_100;
    }

    public class Id
    {
        public static void Check(int id)
        {
            Contract.Requires(id >= 1);
        }

        public static void Check(AudioInputId id)
        {
            // only up to stereo for now
            Check((int)id);
        }

        public static void Check(TrackId id)
        {
            Check((int)id);
        }

        public static void Check(PluginId id)
        {
            Check((int)id);
        }

        public static void Check(ProgramId id)
        {
            Check((int)id);
        }

        public static void Check(PluginInstanceIndex index)
        {
            Check((int)index);
        }
    }

    /// <summary>
    /// Operations on the audio graph as a whole.
    /// </summary>
    /// <remarks>
    /// There is a single "static" audio graph defined here; multiple audio graphs are not (yet?) supported.
    /// All async methods document the state the graph must be in when called, and the state the graph
    /// transitions to on completion.
    ///
    /// Note that this class has private static extern methods for P/Invoking to NowSoundLib,
    /// and public static methods which wrap the P/Invoke methods with more accurate C# types.
    ///
    /// TODO: make this support multiple (non-static) graphs.
    /// </remarks>
    public class NowSoundGraphAPI
    {
        [DllImport("NowSoundLib")]
        static extern NowSoundGraphInfo NowSoundTrack_GetStaticGraphInfo();

        /// <summary>
        /// Get static graph info for validating correct P/Invoke binding.
        /// </summary>
        internal static NowSoundGraphInfo GetStaticGraphInfo()
        {
            return NowSoundTrack_GetStaticGraphInfo();

        }

        [DllImport("NowSoundLib")]
        static extern NowSoundTimeInfo NowSoundGraph_GetStaticTimeInfo();

        /// <summary>
        /// Get static time info for validating correct P/Invoke binding.
        /// </summary>
        internal static TimeInfo GetStaticTimeInfo()
        {
            return new TimeInfo(NowSoundGraph_GetStaticTimeInfo());
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundGraphState NowSoundGraph_State();

        /// <summary>
        /// Get the current state of the audio graph; intended to be efficiently pollable by the client.
        /// This is the only method that may be called in any state whatoever.
        /// </summary>
        public static NowSoundGraphState State()
        {
            return NowSoundGraph_State();
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_InitializeInstance(
            int outputBinCount,
            float centralFrequency,
            int octaveDivisions,
            int centralBinIndex,
            int fftSize);

        /// <summary>
        /// Initialize the audio graph subsystem such that device information can be queried.
        /// Graph must be Uninitialized.  On completion, graph becomes Initialized.
        /// Must be called from message/UI thread. May have a momentary delay as JUCE doesn't support
        /// async initialization.
        /// </summary>
        public static void InitializeInstance(
            int outputBinCount,
            float centralFrequency,
            int octaveDivisions,
            int centralBinIndex,
            int fftSize)
        {
            Contract.Requires(outputBinCount > 0);
            Contract.Requires(centralFrequency > 20); // hz
            Contract.Requires(octaveDivisions > 0);
            Contract.Requires(centralBinIndex > 0);
            Contract.Requires(fftSize > 0);
            // power of two (should check for just one 1-bit but oh well)
            Contract.Requires((fftSize & 0xff) == 0);

            NowSoundGraph_InitializeInstance(
                outputBinCount,
                centralFrequency,
                octaveDivisions,
                centralBinIndex,
                fftSize);
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundGraphInfo NowSoundGraph_Info();

        /// <summary>
        /// Get the graph info for the created graph.
        /// Graph must be Created or Running.
        /// </summary>
        public static NowSoundGraphInfo Info()
        {
            NowSoundGraphInfo info = NowSoundGraph_Info();
            Contract.Assert(info.BitsPerSample == 32);
            Contract.Assert(info.ChannelCount == 2);
            Contract.Assert(info.LatencyInSamples > 0);
            Contract.Assert(info.SampleRate > 0);
            Contract.Assert(info.SamplesPerQuantum > 0);
            return info;
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundLogInfo NowSoundGraph_LogInfo();

        /// <summary>
        /// Get the info about the current log messages.
        /// Graph must be Running.
        /// </summary>
        public static NowSoundLogInfo LogInfo()
        {
            return NowSoundGraph_LogInfo();
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_GetLogMessage(Int32 logMessageIndex, [MarshalAs(UnmanagedType.LPWStr)] StringBuilder buffer, Int32 bufferCapacity);

        /// <summary>
        /// Get the log message with the given index.
        /// </summary>
        public static void GetLogMessage(int logMessageIndex, StringBuilder buffer)
        {
            Contract.Requires(logMessageIndex >= 0);
            Contract.Requires(buffer != null);

            NowSoundGraph_GetLogMessage(logMessageIndex, buffer, buffer.Capacity);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_DropLogMessages(Int32 logMessageIndex);

        /// <summary>
        /// Drop log messages up to (not including) the given index.
        /// </summary>
        public static void DropLogMessages(int logMessageIndex)
        {
            Contract.Requires(logMessageIndex > 0);

            NowSoundGraph_DropLogMessages(logMessageIndex);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_LogConnections();

        /// <summary>
        /// Log all connections in the current JUCE graph.
        /// </summary>
        public static void LogConnections()
        {
            NowSoundGraph_LogConnections();
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundSignalInfo NowSoundGraph_RawInputSignalInfo(AudioInputId audioInputId);

        /// <summary>
        /// Get information about the pre-effects mono signal of an input channel.
        /// Graph must be Running.
        /// </summary>
        /// <returns></returns>
        public static NowSoundSignalInfo RawInputSignalInfo(AudioInputId audioInputId)
        {
            return NowSoundGraph_RawInputSignalInfo(audioInputId);
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundSignalInfo NowSoundGraph_InputSignalInfo(AudioInputId audioInputId);

        /// <summary>
        /// Get information about the post-effects signal of an input channel.
        /// Graph must be Running.
        /// </summary>
        /// <returns></returns>
        public static NowSoundSignalInfo InputSignalInfo(AudioInputId audioInputId)
        {
            return NowSoundGraph_InputSignalInfo(audioInputId);
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundSignalInfo NowSoundGraph_OutputSignalInfo();

        /// <summary>
        /// Get information about the output signal.
        /// Graph must be Running.
        /// </summary>
        /// <returns></returns>
        public static NowSoundSignalInfo OutputSignalInfo()
        {
            return NowSoundGraph_OutputSignalInfo();
        }

        [DllImport("NowSoundLib")]
        static extern float NowSoundGraph_InputPan(AudioInputId audioInputId);

        /// <summary>
        /// Get the pan value of this input (0 = left, 0.5 = center, 1 = right).
        /// </summary>
        public static float InputPan(AudioInputId audioInputId)
        {
            return NowSoundGraph_InputPan(audioInputId);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_SetInputPan(AudioInputId audioInputId, float pan);

        /// <summary>
        /// Set the pan value of this input  (0 = left, 0.5 = center, 1 = right).
        /// </summary>
        public static void SetInputPan(AudioInputId audioInputId, float pan)
        {
            NowSoundGraph_SetInputPan(audioInputId, pan);
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundTimeInfo NowSoundGraph_TimeInfo();

        /// <summary>
        /// Get the current audio graph time.
        /// Graph must be Running.
        /// </summary>
        /// <returns></returns>
        public static TimeInfo TimeInfo()
        {
            TimeInfo result = new TimeInfo(NowSoundGraph_TimeInfo());
            Contract.Assert(result.BeatInMeasure >= 0);
            Contract.Assert(result.BeatsPerMinute >= 0);
            Contract.Assert((float)result.ExactBeat >= 0);
            Contract.Assert((int)result.TimeInSamples >= 0);
            return result;
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_SetBeatsPerMinute(float bpm);

        /// <summary>
        /// Set the BPM of the graph; only functions when no Tracks exist.
        /// Graph must be Running.
        /// </summary>
        public static void SetBeatsPerMinute(float bpm)
        {
            NowSoundGraph_SetBeatsPerMinute(bpm);
        }

        [DllImport("NowSoundLib")]
        static extern bool NowSoundGraph_GetInputFrequencies(AudioInputId audioInputId, float[] floatBuffer, int floatBufferCapacity);

        // Get the current input frequency histogram; LPWSTR must actually reference a float buffer of the
        // same length as the outputBinCount argument passed to InitializeFFT, but must be typed as LPWSTR
        // and must have a capacity represented in two-byte wide characters (to match the P/Invoke style of
        // "pass in StringBuilder", known to work well).
        // Returns true if there was enough data to update the buffer, or false if there was not.
        public static bool GetInputFrequencies(AudioInputId audioInputId, float[] floatBuffer, int floatBufferCapacity)
        {
            Id.Check(audioInputId);

            return NowSoundGraph_GetInputFrequencies(audioInputId, floatBuffer, floatBufferCapacity);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_MessageTick();

        /// <summary>
        /// Call regularly from the "message thread" or client equivalent (e.g. the same thread
        /// from which NowSoundGraph_Initialize was called).
        /// Hack to work around message pumping issues with async updates in our peculiar arrangement of 
        /// P-Invokable JUCE inside of Unity.
        /// </summary>
        public static void MessageTick()
        {
            NowSoundGraph_MessageTick();
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_StartRecording([MarshalAs(UnmanagedType.LPWStr)] string fileName);

        /// <summary>
        /// Start recording to the given file (WAV format); if already recording, this is ignored.
        /// </summary>
        public static void StartRecording(string fileName)
        {
            Contract.Requires(!string.IsNullOrEmpty(fileName));

            NowSoundGraph_StartRecording(fileName);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_StopRecording();

        /// <summary>
        /// Stop recording and close the file; if not recording, this is ignored.
        /// </summary>
        public static void StopRecording()
        {
            NowSoundGraph_StopRecording();
        }

        [DllImport("NowSoundLib")]
        static extern TrackId NowSoundGraph_CreateRecordingTrackAsync(AudioInputId id);

        /// <summary>
        /// Create a new track and begin recording.
        /// </summary>
        /// <remarks>
        /// Graph must be Running.
        /// TrackId returned from native code is incremented for TrackId on this end, to ensure any default
        /// TrackId values are treated as uninitialized.
        /// </remarks>
        public static TrackId CreateRecordingTrackAsync(AudioInputId id)
        {
            Id.Check(id);

            TrackId result = NowSoundGraph_CreateRecordingTrackAsync(id);
            Id.Check(result);
            return result;
        }


        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_DeleteTrack(TrackId trackId);

        /// <summary>
        /// Delete this Track; after this, all methods become invalid to call (contract failure).
        /// </summary>
        public static void DeleteTrack(TrackId trackId)
        {
            Id.Check(trackId);

            NowSoundGraph_DeleteTrack(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_AddPluginSearchPath([MarshalAs(UnmanagedType.LPWStr)] string path);

        /// <summary>
        /// Plugin searching requires setting paths to search.
        /// TODO: make this use the idiom for passing in strings rather than StringBuilders.
        /// </summary>
        public static void AddPluginSearchPath(string path)
        {
            Contract.Requires(!string.IsNullOrEmpty(path));
            Contract.Requires(Directory.Exists(path));

            NowSoundGraph_AddPluginSearchPath(path);
        }

        [DllImport("NowSoundLib")]
        static extern bool NowSoundGraph_SearchPluginsSynchronously();

        /// <summary>
        /// After setting one or more search paths, actually search.
        /// TODO: make this asynchronous.
        /// Returns true if no errors in searching, or false if there were errors (printed to debug log, hopefully).
        /// </summary>
        public static bool SearchPluginsSynchronously()
        {
            return NowSoundGraph_SearchPluginsSynchronously();
        }

        [DllImport("NowSoundLib")]
        static extern int NowSoundGraph_PluginCount();

        /// <summary>
        /// How many plugins?
        /// </summary>
        public static int PluginCount()
        {
            int count = NowSoundGraph_PluginCount();
            Contract.Assert(count >= 0);
            return count;
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_PluginName(PluginId pluginId, [MarshalAs(UnmanagedType.LPWStr)] StringBuilder buffer, Int32 bufferCapacity);

        /// <summary>
        /// Get the name of the Nth plugin. Note that IDs are 1-based.
        /// </summary>
        public static void PluginName(PluginId pluginId, [MarshalAs(UnmanagedType.LPWStr)] StringBuilder buffer)
        {
            Id.Check(pluginId);
            Contract.Requires(buffer != null);
            Contract.Requires(buffer.Capacity > 0);

            NowSoundGraph_PluginName(pluginId, buffer, buffer.Capacity);
        }

        [DllImport("NowSoundLib")]
        static extern bool NowSoundGraph_LoadPluginPrograms(PluginId pluginId, [MarshalAs(UnmanagedType.LPWStr)] string path);

        /// <summary>
        /// Load all the programs for the given plugin from the given directory.
        /// </summary>
        public static bool LoadPluginPrograms(PluginId pluginId, [MarshalAs(UnmanagedType.LPWStr)] string path)
        {
            Id.Check(pluginId);
            Contract.Requires(!string.IsNullOrEmpty(path));
            Contract.Requires(Directory.Exists(path));

            return NowSoundGraph_LoadPluginPrograms(pluginId, path);
        }

        [DllImport("NowSoundLib")]
        static extern int NowSoundGraph_PluginProgramCount(PluginId pluginId);

        /// <summary>
        /// Get the number of programs for the given plugin.
        /// </summary>
        public static int PluginProgramCount(PluginId pluginId)
        {
            Id.Check(pluginId);

            int result = NowSoundGraph_PluginProgramCount(pluginId);
            Contract.Assert(result >= 0);
            return result;
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_PluginProgramName(PluginId pluginId, ProgramId programId, [MarshalAs(UnmanagedType.LPWStr)] StringBuilder buffer, Int32 bufferCapacity);

        /// <summary>
        /// Get the name of the specified plugin's program.  Note that IDs are 1-based.
        /// </summary>
        public static void PluginProgramName(PluginId pluginId, ProgramId programId, [MarshalAs(UnmanagedType.LPWStr)] StringBuilder buffer)
        {
            Id.Check(pluginId);
            Id.Check(programId);
            Contract.Requires(buffer != null);
            Contract.Requires(buffer.Capacity > 0);

            NowSoundGraph_PluginProgramName(pluginId, programId, buffer, buffer.Capacity);
        }

        // Add an instance of the given plugin on the given track.
        [DllImport("NowSoundLib")]
        static extern PluginInstanceIndex NowSoundGraph_AddInputPluginInstance(AudioInputId audioInputId, PluginId pluginId, ProgramId programId, Int32 dryWet_0_100);

        public static PluginInstanceIndex AddInputPluginInstance(AudioInputId audioInputId, PluginId pluginId, ProgramId programId, int dryWet_0_100)
        {
            Id.Check(audioInputId);
            Id.Check(pluginId);
            Id.Check(programId);
            Contract.Requires(dryWet_0_100 >= 0);
            Contract.Requires(dryWet_0_100 <= 100);

            PluginInstanceIndex result = NowSoundGraph_AddInputPluginInstance(audioInputId, pluginId, programId, dryWet_0_100);
            Id.Check(result);
            return result;
        }

        [DllImport("NowSoundLib")]
        static extern int NowSoundGraph_GetInputPluginInstanceCount(AudioInputId audioInputId);


        /// <summary>
        /// Get the number of plugin instances on this input.
        /// </summary>
        public static int GetInputPluginInstanceCount(AudioInputId audioInputId)
        {
            Id.Check(audioInputId);

            int result = NowSoundGraph_GetInputPluginInstanceCount(audioInputId);
            Contract.Assert(result >= 0);
            return result;
        }

        [DllImport("NowSoundLib")]
        static extern PluginInstanceInfo NowSoundGraph_GetInputPluginInstanceInfo(AudioInputId audioInputId, PluginInstanceIndex index);


        /// <summary>
        /// Get info about a plugin instance on an input.
        /// </summary>
        public static PluginInstanceInfo GetInputPluginInstanceInfo(AudioInputId audioInputId, PluginInstanceIndex index)
        {
            Id.Check(audioInputId);
            Id.Check(index);

            return NowSoundGraph_GetInputPluginInstanceInfo(audioInputId, index);
        }


        // Set the dry/wet balance on the given plugin.
        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_SetInputPluginInstanceDryWet(AudioInputId audioInputId, PluginInstanceIndex pluginInstanceIndex, int dryWet_0_100);

        public static void SetInputPluginInstanceDryWet(AudioInputId audioInputId, PluginInstanceIndex pluginInstanceIndex, int dryWet_0_100)
        {
            Id.Check(audioInputId);
            Id.Check(pluginInstanceIndex);
            Contract.Requires(dryWet_0_100 >= 0);
            Contract.Requires(dryWet_0_100 <= 100);

            NowSoundGraph_SetInputPluginInstanceDryWet(audioInputId, pluginInstanceIndex, dryWet_0_100);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_DeleteInputPluginInstance(AudioInputId audioInputId, PluginInstanceIndex index);

        /// <summary>
        /// Delete a plugin instance.
        /// </summary>
        public static void DeleteInputPluginInstance(AudioInputId audioInputId, PluginInstanceIndex pluginInstanceIndex)
        {
            Id.Check(audioInputId);
            Id.Check(pluginInstanceIndex);

            NowSoundGraph_DeleteInputPluginInstance(audioInputId, pluginInstanceIndex);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_ShutdownInstance();

        // Shut down the graph and dispose its singleton instance.  On completion, state becomes Uninitialized.
        // Must be called from message/UI thread. May have a momentary delay as JUCE doesn't support
        // async initialization.
        public static void ShutdownInstance()
        {
            NowSoundGraph_ShutdownInstance();
        }

    }

    // Interface used to invoke operations on a particular audio track.
    // TODO: update native code to never return a zero/default TrackId, and then remove the "trackId - 1" code everywhere here.
    public class NowSoundTrackAPI
    {
        [DllImport("NowSoundLib")]
        static extern NowSoundTrackInfo NowSoundTrack_GetStaticTrackInfo();

        // In what state is this track?
        public static TrackInfo GetStaticTrackInfo()
        {
            return new TrackInfo(NowSoundTrack_GetStaticTrackInfo());
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundTrackState NowSoundTrack_State(TrackId trackId);

        // In what state is this track?
        public static NowSoundTrackState State(TrackId trackId)
        {
            Id.Check(trackId);

            NowSoundTrackState result = NowSoundTrack_State(trackId);
            Contract.Assert(result >= NowSoundTrackState.TrackUninitialized);
            Contract.Assert(result <= NowSoundTrackState.TrackLooping);
            return result;
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundTrackInfo NowSoundTrack_Info(TrackId trackId);

        // The current timing information for this Track.
        public static TrackInfo Info(TrackId trackId)
        {
            Id.Check(trackId);

            return new TrackInfo(NowSoundTrack_Info(trackId));
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundSignalInfo NowSoundTrack_SignalInfo(TrackId trackId);

        // The current output signal information for this Track.
        public static NowSoundSignalInfo SignalInfo(TrackId trackId)
        {
            Id.Check(trackId);

            return NowSoundTrack_SignalInfo(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundTrack_FinishRecording(TrackId trackId);

        // The user wishes the track to finish recording now.
        // Contractually requires State == NowSoundTrack_State.Recording.
        public static void FinishRecording(TrackId trackId)
        {
            Id.Check(trackId);

            NowSoundTrack_FinishRecording(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern bool NowSoundTrack_GetFrequencies(TrackId trackId, float[] floatBuffer, int floatBufferCapacity);

        // Get the current track frequency histogram; LPWSTR must actually reference a float buffer of the
        // same length as the outputBinCount argument passed to InitializeFFT, but must be typed as LPWSTR
        // and must have a capacity represented in two-byte wide characters (to match the P/Invoke style of
        // "pass in StringBuilder", known to work well).
        // Returns true if there was enough data to update the buffer, or false if there was not.
        public static bool GetFrequencies(TrackId trackId, float[] floatBuffer)
        {
            Id.Check(trackId);
            Contract.Requires(floatBuffer != null);

            return NowSoundTrack_GetFrequencies(trackId, floatBuffer, floatBuffer.Length);
        }

        [DllImport("NowSoundLib")]
        static extern bool NowSoundTrack_IsMuted(TrackId trackId);

        // True if this is muted.
        // 
        // Note that something can be in FinishRecording state but still be muted, if the user is fast!
        // Hence this is a separate flag, not represented as a NowSoundTrack_State.
        public static bool IsMuted(TrackId trackId)
        {
            Id.Check(trackId);

            return NowSoundTrack_IsMuted(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundTrack_SetIsMuted(TrackId trackId, bool isMuted);

        public static void SetIsMuted(TrackId trackId, bool isMuted)
        {
            Id.Check(trackId);

            NowSoundTrack_SetIsMuted(trackId, isMuted);
        }

        [DllImport("NowSoundLib")]
        static extern float NowSoundTrack_Pan(TrackId trackId);

        public static float Pan(TrackId trackId)
        {
            Id.Check(trackId);

            return NowSoundTrack_Pan(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundTrack_SetPan(TrackId trackId, float pan);

        public static void SetPan(TrackId trackId, float pan)
        {
            Id.Check(trackId);

            NowSoundTrack_SetPan(trackId, pan);
        }

        [DllImport("NowSoundLib")]
        static extern float NowSoundTrack_Volume(TrackId trackId);

        public static float Volume(TrackId trackId)
        {
            Id.Check(trackId);

            return NowSoundTrack_Volume(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundTrack_SetVolume(TrackId trackId, float volume);

        public static void SetVolume(TrackId trackId, float volume)
        {
            Id.Check(trackId);

            NowSoundTrack_SetVolume(trackId, volume);
        }

        // Add an instance of the given plugin on the given track.
        [DllImport("NowSoundLib")]
        static extern PluginInstanceIndex NowSoundTrack_AddPluginInstance(TrackId trackId, PluginId pluginId, ProgramId programId, int dryWet_0_100);

        public static PluginInstanceIndex AddPluginInstance(TrackId trackId, PluginId pluginId, ProgramId programId, int dryWet_0_100)
        {
            Id.Check(trackId);
            Id.Check(pluginId);
            Id.Check(programId);
            Contract.Requires(dryWet_0_100 >= 0);
            Contract.Requires(dryWet_0_100 <= 100);

            PluginInstanceIndex result = NowSoundTrack_AddPluginInstance(trackId, pluginId, programId, dryWet_0_100);
            Id.Check(result);
            return result;
        }

        [DllImport("NowSoundLib")]
        static extern int NowSoundTrack_GetPluginInstanceCount(TrackId trackId);


        /// <summary>
        /// Get the number of plugin instances on this track.
        /// </summary>
        public static int GetPluginInstanceCount(TrackId trackId)
        {
            Id.Check(trackId);

            int result = NowSoundTrack_GetPluginInstanceCount(trackId);
            Contract.Assert(result >= 0);
            return result;
        }

        [DllImport("NowSoundLib")]
        static extern PluginInstanceInfo NowSoundTrack_GetPluginInstanceInfo(TrackId trackId, PluginInstanceIndex index);


        /// <summary>
        /// Get info about a plugin instance on a track.
        /// </summary>
        public static PluginInstanceInfo GetInputPluginInstanceInfo(TrackId trackId, PluginInstanceIndex index)
        {
            Id.Check(trackId);

            return NowSoundTrack_GetPluginInstanceInfo(trackId, index);
        }

        // Set the dry/wet balance on the given plugin.
        [DllImport("NowSoundLib")]
        static extern void NowSoundTrack_SetPluginInstanceDryWet(TrackId trackId, PluginInstanceIndex pluginInstanceIndex, int dryWet_0_100);

        public static void SetPluginInstanceDryWet(TrackId trackId, PluginInstanceIndex pluginInstanceIndex, int dryWet_0_100)
        {
            Id.Check(trackId);
            Id.Check(pluginInstanceIndex);
            Contract.Requires(dryWet_0_100 >= 0);
            Contract.Requires(dryWet_0_100 <= 100);

            NowSoundTrack_SetPluginInstanceDryWet(trackId, pluginInstanceIndex, dryWet_0_100);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundTrack_DeletePluginInstance(TrackId trackId, PluginInstanceIndex index);

        // Delete a plugin instance.
        public static void DeletePluginInstance(TrackId trackId, PluginInstanceIndex pluginInstanceIndex)
        {
            Id.Check(trackId);
            Id.Check(pluginInstanceIndex);

            NowSoundTrack_DeletePluginInstance(trackId, pluginInstanceIndex);
        }
    }
}
