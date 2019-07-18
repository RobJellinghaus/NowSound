// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

// P/Invoke wrapper types and classes for NowSoundLib.
namespace NowSoundLib
{
    public struct NowSoundLogInfo
    {
        public Int32 LogMessageCount;
    }

    // TODO: actually use this over P/Invoke via StringBuilder marshaling (avoiding all lifetime issues nicely)
    public struct DeviceInfo
    {
        public readonly string Id;
        public readonly string Name;
        // Would be nice if this was IReadOnlyDictionary but C# 4 don't do that
        public readonly Dictionary<string, object> Properties;

        /// <summary>
        /// Construct a DeviceInfo; it will directly reference the given dictionary (no copying).
        /// </summary>
        public DeviceInfo(string id, string name, Dictionary<string, object> properties)
        {
            Id = id;
            Name = name;
            Properties = properties;
        }
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
        public Int32 BeatInMeasure;
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
        // The number of AudioInputs defined in the graph.
        //public readonly int AudioInputCount;
        // The number of samples elapsed since the audio graph started.
        public readonly Time<AudioSample> TimeInSamples;
        // The exact current beat (including fractional part; truncate to get integral beat count).
        public readonly ContinuousDuration<Beat> ExactBeat;
        // The current BPM of the graph.
        public readonly float BeatsPerMinute;
        // The current position in the measure. (e.g. 4/4 time = this ranges from 0 to 3)
        public readonly Int32 BeatInMeasure;

        internal TimeInfo(NowSoundTimeInfo pinvokeTimeInfo)
        {
            //AudioInputCount = pinvokeTimeInfo.AudioInputCount;
            TimeInSamples = pinvokeTimeInfo.TimeInSamples;
            ExactBeat = pinvokeTimeInfo.ExactBeat;
            BeatsPerMinute = pinvokeTimeInfo.BeatsPerMinute;
            BeatInMeasure = pinvokeTimeInfo.BeatInMeasure;
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
    /// 1-based ID for a particular instance of a plugin instantiated on a track.
    /// </summary>
    public enum TrackPluginInstanceId
    {
        Undefined = 0
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
        static extern void NowSoundGraph_InitializeInstance();

        /// <summary>
        /// Initialize the audio graph subsystem such that device information can be queried.
        /// Graph must be Uninitialized.  On completion, graph becomes Initialized.
        /// Must be called from message/UI thread. May have a momentary delay as JUCE doesn't support
        /// async initialization.
        /// </summary>
        public static void InitializeInstance()
        {
            NowSoundGraph_InitializeInstance();
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundGraphInfo NowSoundGraph_Info();

        /// <summary>
        /// Get the graph info for the created graph.
        /// Graph must be Created or Running.
        /// </summary>
        public static NowSoundGraphInfo Info()
        {
            return NowSoundGraph_Info();
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
        /// Get the current track frequency histogram.
        /// Returns true if there was enough data to update the buffer, or false if there was not.
        /// </summary>
        public static void GetLogMessage(int logMessageIndex, StringBuilder buffer)
        {
            NowSoundGraph_GetLogMessage(logMessageIndex, buffer, buffer.Capacity);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_DropLogMessages(Int32 logMessageIndex);

        /// <summary>
        /// Get the current track frequency histogram.
        /// Returns true if there was enough data to update the buffer, or false if there was not.
        /// </summary>
        /// <param name="logMessageIndex"></param>
        public static void DropLogMessages(int logMessageIndex)
        {
            NowSoundGraph_DropLogMessages(logMessageIndex);
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundSignalInfo NowSoundGraph_OutputSignalInfo();

        /// <summary>
        /// Get information about the current (final mix) output signal.
        /// Graph must be Running.
        /// </summary>
        /// <returns></returns>
        public static NowSoundSignalInfo OutputSignalInfo()
        {
            return NowSoundGraph_OutputSignalInfo();
        }

        // TODO: add query methods for device ID & name taking StringBuilder

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_InitializeDeviceInputs(int deviceIndex);

        /// <summary>
        /// Initialize the given device, given its index (as passed to InputDeviceInfo); returns the AudioInputId of the
        /// input device.  If the input device has multiple channels, multiple consecutive AudioInputIds will be allocated,
        /// but only the first will be returned.
        /// </summary>
        public static void InitializeDeviceInputs(int deviceIndex)
        {
            NowSoundGraph_InitializeDeviceInputs(deviceIndex);
        }

        // Initialize the FFT subsystem, which for now must be done before graph creation.
        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_InitializeFFT(
            int outputBinCount,
            float centralFrequency,
            int octaveDivisions,
            int centralBinIndex,
            int fftSize);

        public static void InitializeFFT(
            // How many output bins in the (logarithmic) frequency histogram?
            int outputBinCount,
            // What central frequency to use for the histogram?
            float centralFrequency,
            // How many divisions to make in each octave?
            int octaveDivisions,
            // Which bin index should be centered on centralFrequency?
            int centralBinIndex,
            // How many samples as input to and output from the FFT?
            int fftSize)
        {
            NowSoundGraph_InitializeFFT(
                outputBinCount,
                centralFrequency,
                octaveDivisions,
                centralBinIndex,
                fftSize);
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
            return new TimeInfo(NowSoundGraph_TimeInfo());
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_PlayUserSelectedSoundFileAsync();

        /// <summary>
        /// Play a user-selected sound file.
        /// Graph must be Started.
        /// </summary>
        public static void PlayUserSelectedSoundFileAsync()
        {
            NowSoundGraph_PlayUserSelectedSoundFileAsync();
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
        static extern TrackId NowSoundGraph_CreateRecordingTrackAsync(AudioInputId id);

        /// <summary>
        /// Create a new track and begin recording.
        /// Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
        /// TrackId returned from native code is incremented for TrackId on this end, to ensure any default
        /// TrackId values are treated as uninitialized.
        /// </summary>
        public static TrackId CreateRecordingTrackAsync(AudioInputId id)
        {
            return NowSoundGraph_CreateRecordingTrackAsync(id);
        }


        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_DeleteTrack(TrackId trackId);

        /// <summary>
        /// Delete this Track; after this, all methods become invalid to call (contract failure).
        /// </summary>
        public static void DeleteTrack(TrackId trackId)
        {
            NowSoundGraph_DeleteTrack(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_AddPluginSearchPath([MarshalAs(UnmanagedType.LPWStr)] StringBuilder buffer, Int32 bufferCapacity);

        /// <summary>
        /// Plugin searching requires setting paths to search.
        /// TODO: make this use the idiom for passing in strings rather than StringBuilders.
        /// </summary>
        public static void AddPluginSearchPath([MarshalAs(UnmanagedType.LPWStr)] StringBuilder buffer, Int32 bufferCapacity)
        {
            NowSoundGraph_AddPluginSearchPath(buffer, bufferCapacity);
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
            return NowSoundGraph_PluginCount();
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_PluginName(PluginId pluginId, [MarshalAs(UnmanagedType.LPWStr)] StringBuilder buffer, Int32 bufferCapacity);

        /// <summary>
        /// Get the name of the Nth plugin. Note that IDs are 1-based.
        /// </summary>
        public static void PluginName(PluginId pluginId, [MarshalAs(UnmanagedType.LPWStr)] StringBuilder buffer, Int32 bufferCapacity)
        {
            NowSoundGraph_PluginName(pluginId, buffer, bufferCapacity);
        }

        [DllImport("NowSoundLib")]
        static extern int NowSoundGraph_PluginProgramCount(PluginId pluginId);

        /// <summary>
        /// Get the number of programs for the given plugin.
        /// </summary>
        public static int PluginProgramCount(PluginId pluginId)
        {
            return NowSoundGraph_PluginProgramCount(pluginId);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundGraph_PluginProgramName(PluginId pluginId, ProgramId programId, [MarshalAs(UnmanagedType.LPWStr)] StringBuilder buffer, Int32 bufferCapacity);

        /// <summary>
        /// Get the name of the specified plugin's program.  Note that IDs are 1-based.
        /// </summary>
        public static void PluginProgramName(PluginId pluginId, ProgramId programId, [MarshalAs(UnmanagedType.LPWStr)] StringBuilder buffer, Int32 bufferCapacity)
        {
            NowSoundGraph_PluginProgramName(pluginId, programId, buffer, bufferCapacity);
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
            return NowSoundTrack_State(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundTrackInfo NowSoundTrack_Info(TrackId trackId);

        // The current timing information for this Track.
        public static TrackInfo Info(TrackId trackId)
        {
            return new TrackInfo(NowSoundTrack_Info(trackId));
        }

        [DllImport("NowSoundLib")]
        static extern NowSoundSignalInfo NowSoundTrack_SignalInfo(TrackId trackId);

        // The current output signal information for this Track.
        public static NowSoundSignalInfo SignalInfo(TrackId trackId)
        {
            return NowSoundTrack_SignalInfo(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundTrack_FinishRecording(TrackId trackId);

        // The user wishes the track to finish recording now.
        // Contractually requires State == NowSoundTrack_State.Recording.
        public static void FinishRecording(TrackId trackId)
        {
            NowSoundTrack_FinishRecording(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern bool NowSoundTrack_GetFrequencies(TrackId trackId, float[] floatBuffer, int floatBufferCapacity);

        // Get the current track frequency histogram; LPWSTR must actually reference a float buffer of the
        // same length as the outputBinCount argument passed to InitializeFFT, but must be typed as LPWSTR
        // and must have a capacity represented in two-byte wide characters (to match the P/Invoke style of
        // "pass in StringBuilder", known to work well).
        // Returns true if there was enough data to update the buffer, or false if there was not.
        public static bool GetFrequencies(TrackId trackId, float[] floatBuffer, int floatBufferCapacity)
        {
            return NowSoundTrack_GetFrequencies(trackId, floatBuffer, floatBufferCapacity);
        }

        [DllImport("NowSoundLib")]
        static extern bool NowSoundTrack_IsMuted(TrackId trackId);

        // True if this is muted.
        // 
        // Note that something can be in FinishRecording state but still be muted, if the user is fast!
        // Hence this is a separate flag, not represented as a NowSoundTrack_State.
        public static bool IsMuted(TrackId trackId)
        {
            return NowSoundTrack_IsMuted(trackId);
        }

        [DllImport("NowSoundLib")]
        static extern void NowSoundTrack_SetIsMuted(TrackId trackId, bool isMuted);

        public static void SetIsMuted(TrackId trackId, bool isMuted)
        {
            NowSoundTrack_SetIsMuted(trackId, isMuted);
        }

        // Add an instance of the given plugin on the given track.
        [DllImport("NowSoundLib")]
        static extern TrackPluginInstanceId NowSoundTrack_AddPlugin(TrackId trackId, PluginId pluginId, ProgramId programId);

        public static TrackPluginInstanceId AddPlugin(TrackId trackId, PluginId pluginId, ProgramId programId)
        {
            return NowSoundTrack_AddPlugin(trackId, pluginId, programId);
        }

        // Set the dry/wet balance on the given plugin.
        [DllImport("NowSoundLib")]
        static extern void NowSoundTrack_SetPluginDryWet(TrackId trackId, TrackPluginInstanceId pluginInstanceId, int dryWet_0_100);

        public static void SetPluginDryWet(TrackId trackId, TrackPluginInstanceId pluginInstanceId, int dryWet_0_100)
        {
            NowSoundTrack_SetPluginDryWet(trackId, pluginInstanceId, dryWet_0_100);
        }
    }
}
