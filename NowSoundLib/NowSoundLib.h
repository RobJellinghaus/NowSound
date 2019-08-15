// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

// This is the external include file for NowSoundLib.
// This should be the only file #included from any external project seeking to invoke NowSoundLib.

#pragma once

#include "stdint.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "NowSoundLibTypes.h"
#include "SliceStream.h"

namespace NowSound
{
    // All external methods here are static and use C linkage, for P/Invokability.
    // AudioGraph object references are passed by integer ID; lifecycle is documented
    // per-API.
    extern "C"
    {
        // Operations on the audio graph as a whole.
        //
        // There is a single "static" audio graph defined here; multiple audio graphs are not (yet?) supported.
        //
        // All async methods document the state the graph must be in when called, and the state the graph
        // transitions to on completion.
        //
        // Note that this API is not thread-safe; methods are not re-entrant and must be called sequentially.
        //
        // TODO: make this support multiple (non-static) graphs.
        // TODO: consider ditching the wrapper class here and having top-level global functions so extern "C" would actually work properly.

        // Test method only: get a predefined NowSound_GraphInfo instance to test P/Invoke serialization.
        __declspec(dllexport) NowSoundGraphInfo NowSoundGraph_GetStaticGraphInfo();

        // Test method only: get a predefined NowSound_TimeInfo instance to test P/Invoke serialization.
        __declspec(dllexport) NowSoundTimeInfo NowSoundGraph_GetStaticTimeInfo();

        // Get the current state of the audio graph; intended to be efficiently pollable by the client.
        // This is the only method that may be called in any state whatoever.
        __declspec(dllexport) NowSoundGraphState NowSoundGraph_State();

        // Get the number of log messages.
        __declspec(dllexport) NowSoundLogInfo NowSoundGraph_LogInfo();

        // Get the message with a particular (zero-based) index; must be between (inclusive) the indices last returned from NowSoundGraph_LogInfo().
        // Note that calls to this method must never be interleaved with calls to the NowSoundGraph_DropLogMessagesUpTo method; these
        // methods are not thread-safe with respect to each other.
        __declspec(dllexport) void NowSoundGraph_GetLogMessage(int32_t logMessageIndex, LPWSTR wcharBuffer, int32_t bufferCapacity);

        // Drop this many log messages.
        __declspec(dllexport) void NowSoundGraph_DropLogMessages(int32_t messageCountToDrop);

        // Log the current JUCE audio processor graph connections.
        __declspec(dllexport) void NowSoundGraph_LogConnections();

        // Initialize the audio graph subsystem such that device information can be queried.
        // Graph must be Uninitialized.  On completion, graph becomes Initialized.
        __declspec(dllexport) void NowSoundGraph_InitializeInstance(
            // How many output bins in the (logarithmic) frequency histogram?
            int outputBinCount,
            // What central frequency to use for the histogram?
            float centralFrequency,
            // How many divisions to make in each octave?
            int octaveDivisions,
            // Which bin index should be centered on centralFrequency?
            int centralBinIndex,
            // How many samples as input to and output from the FFT?
            int fftSize);

        // Get the info for the created graph.
        // Graph must be at least Created.
        __declspec(dllexport) NowSoundGraphInfo NowSoundGraph_Info();

        // Get the current pre-effects (raw) signal from the given input.
        __declspec(dllexport) NowSoundSignalInfo NowSoundGraph_RawInputSignalInfo(AudioInputId audioInputId);

        // Get the current info for the post-effects signal from the given input.
        __declspec(dllexport) NowSoundSignalInfo NowSoundGraph_InputSignalInfo(AudioInputId audioInputId);

        // Get the current info for the graph's final mixed output (channel 0 only, currently).
        __declspec(dllexport) NowSoundSignalInfo NowSoundGraph_OutputSignalInfo();

        // Get the ID of the given device.
        // Graph must be at least Initialized.
        // JUCETODO: __declspec(dllexport) void NowSoundGraph_InputDeviceId(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity);

        // Get the name of the given device.
        // Graph must be at least Initialized.
        // JUCETODO: __declspec(dllexport) void NowSoundGraph_InputDeviceName(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity);

        // Initialize the given device, given its index (as passed to InputDeviceInfo); returns the AudioInputId of the
        // input device.  If the input device has multiple channels, multiple consecutive AudioInputIds will be allocated,
        // but only the first will be returned.
        // JUCETODO: __declspec(dllexport) void NowSoundGraph_InitializeDeviceInputs(int deviceIndex);

        // Get the time info for the created graph.
        // Graph must be at least Created; time will not be running until the graph is Running.
        __declspec(dllexport) NowSoundTimeInfo NowSoundGraph_TimeInfo();

        // Get the info for the specified input.
        // Graph must be at least Created; time will not be running until the graph is Running.
        __declspec(dllexport) NowSoundSpatialParameters NowSoundGraph_InputInfo(AudioInputId inputId);

        // Get the current input frequency histogram (post-effects); LPWSTR must actually reference a float buffer of the
        // same length as the outputBinCount argument passed to InitializeFFT, but must be typed as LPWSTR
        // and must have a capacity represented in two-byte wide characters (to match the P/Invoke style of
        // "pass in StringBuilder", known to work well).
        __declspec(dllexport) void NowSoundGraph_GetInputFrequencies(AudioInputId audioInputId, void* floatBuffer, int32_t floatBufferCapacity);

        // Play a user-selected sound file.
        // Graph must be Started.
        __declspec(dllexport) void NowSoundGraph_PlayUserSelectedSoundFileAsync();

        // Create a new track and begin recording.
        __declspec(dllexport) TrackId NowSoundGraph_CreateRecordingTrackAsync(AudioInputId audioInputId);

        // Delete this Track; after this, calling any methods with this TrackID will cause contract failure.
        void __declspec(dllexport) NowSoundGraph_DeleteTrack(TrackId trackId);

        // Call this regularly from the "message thread".
        // Terrible hack to work around message pump issues.
        __declspec(dllexport) void NowSoundGraph_MessageTick();

        // Plugin searching requires setting paths to search.
        // TODO: make this use the idiom for passing in strings rather than StringBuilders.
        __declspec(dllexport) void NowSoundGraph_AddPluginSearchPath(LPWSTR wcharBuffer, int32_t bufferCapacity);

        // After setting one or more search paths, actually search.
        // TODO: make this asynchronous.
        // Returns true if no errors in searching, or false if there were errors (printed to debug log, hopefully).
        __declspec(dllexport) bool NowSoundGraph_SearchPluginsSynchronously();

        // How many plugins?
        __declspec(dllexport) int NowSoundGraph_PluginCount();

        // Get the name of the Nth plugin. Note that IDs are 1-based.
        __declspec(dllexport) void NowSoundGraph_PluginName(PluginId pluginId, LPWSTR wcharBuffer, int32_t bufferCapacity);

        // Load programs for the given plugin from the given directory.
        __declspec(dllexport) bool NowSoundGraph_LoadPluginPrograms(PluginId pluginId, LPWSTR pathnameBuffer);

        // Get the number of programs for the given plugin.  (Call this after loading.)
        __declspec(dllexport) int NowSoundGraph_PluginProgramCount(PluginId pluginId);

        // Get the name of the specified plugin's program.  Note that IDs are 1-based.
        __declspec(dllexport) void NowSoundGraph_PluginProgramName(PluginId pluginId, ProgramId programId, LPWSTR wcharBuffer, int32_t bufferCapacity);

        // Add an instance of the given plugin on the given input.
        __declspec(dllexport) PluginInstanceIndex NowSoundGraph_AddInputPluginInstance(AudioInputId audioInputId, PluginId pluginId, ProgramId programId, int32_t dryWet_0_100);
        // Set the dry/wet balance on the given plugin.
        __declspec(dllexport) void NowSoundGraph_SetInputPluginInstanceDryWet(AudioInputId audioInputId, PluginInstanceIndex pluginInstanceIndex, int32_t dryWet_0_100);
        // Delete the given plugin instance; note that this will effectively renumber all subsequent instances.
        __declspec(dllexport) void NowSoundGraph_DeleteInputPluginInstance(AudioInputId audioInputId, PluginInstanceIndex pluginInstanceIndex);

        // Tear down the whole graph.
        // Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
        __declspec(dllexport) void NowSoundGraph_ShutdownInstance();
    }

    extern "C"
    {
        // Interface used to invoke operations on a particular audio track.
        //
        // Note that this API is not thread-safe; methods are not re-entrant and must be called sequentially,
        // not concurrently.

        // Test method only: get a predefined NowSoundTrack_TrackTimeInfo instance to test P/Invoke serialization.
        __declspec(dllexport) NowSoundTrackInfo NowSoundTrack_GetStaticTrackInfo();

        // In what state is this track?
        __declspec(dllexport) NowSoundTrackState NowSoundTrack_State(TrackId trackId);

        // The current timing information for this Track.
        __declspec(dllexport) NowSoundTrackInfo NowSoundTrack_Info(TrackId trackId);

        // The current signal information for this Track (tracking the mono input channel).
        __declspec(dllexport) NowSoundSignalInfo NowSoundTrack_SignalInfo(TrackId trackId);

        // The user wishes the track to finish recording now, or at least when its quantized duration is reached.
        // Contractually requires State == NowSoundTrack_State.Recording.
        __declspec(dllexport) void NowSoundTrack_FinishRecording(TrackId trackId);

        // Get the current track frequency histogram (post-effects); LPWSTR must actually reference a float buffer of the
        // same length as the outputBinCount argument passed to InitializeFFT, but must be typed as LPWSTR
        // and must have a capacity represented in two-byte wide characters (to match the P/Invoke style of
        // "pass in StringBuilder", known to work well).
        __declspec(dllexport) void NowSoundTrack_GetFrequencies(TrackId trackId, void* floatBuffer, int32_t floatBufferCapacity);

        // True if this is muted.
        // 
        // Note that something can be in FinishRecording state but still be muted, if the user is fast!
        // Hence this is a separate flag, not represented as a NowSoundTrack_State.
        __declspec(dllexport) bool NowSoundTrack_IsMuted(TrackId trackId);
        __declspec(dllexport) void NowSoundTrack_SetIsMuted(TrackId trackId, bool isMuted);

        // Add an instance of the given plugin on the given track.
        __declspec(dllexport) PluginInstanceIndex NowSoundTrack_AddPluginInstance(TrackId trackId, PluginId pluginId, ProgramId programId, int32_t dryWet_0_100);
        // Set the dry/wet balance on the given plugin. TODO: implement this!
        __declspec(dllexport) void NowSoundTrack_SetPluginInstanceDryWet(TrackId trackId, PluginInstanceIndex PluginInstanceIndex, int32_t dryWet_0_100);
        // Delete the given plugin instance; note that this will effectively renumber all subsequent instances.
        __declspec(dllexport) void NowSoundTrack_DeletePluginInstance(TrackId trackId, PluginInstanceIndex PluginInstanceIndex);
    };
}
