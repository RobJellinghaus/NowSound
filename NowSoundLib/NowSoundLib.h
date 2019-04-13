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
    // Callbacks are implemented by passing statically invokable callback hooks
    // together with callback IDs.
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

		// Initialize the audio graph subsystem such that device information can be queried.
		// Graph must be Uninitialized.  On completion, graph becomes Initialized.
		__declspec(dllexport) void NowSoundGraph_InitializeInstance();

		// Get the info for the created graph.
		// Graph must be at least Created.
		__declspec(dllexport) NowSoundGraphInfo NowSoundGraph_Info();

        // Get the current info for the graph's final mixed output.
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

		// Initialize the FFT subsystem, which for now must be done before graph creation.
		__declspec(dllexport) void NowSoundGraph_InitializeFFT(
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

		// Get the time info for the created graph.
		// Graph must be at least Created; time will not be running until the graph is Running.
		__declspec(dllexport) NowSoundTimeInfo NowSoundGraph_TimeInfo();

		// Get the info for the specified input.
		// Graph must be at least Created; time will not be running until the graph is Running.
		__declspec(dllexport) NowSoundInputInfo NowSoundGraph_InputInfo(AudioInputId inputId);

        // Play a user-selected sound file.
        // Graph must be Started.
        __declspec(dllexport) void NowSoundGraph_PlayUserSelectedSoundFileAsync();

        // Tear down the whole graph.
        // Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
        __declspec(dllexport) void NowSoundGraph_ShutdownInstance();

        // Create a new track and begin recording.
        __declspec(dllexport) TrackId NowSoundGraph_CreateRecordingTrackAsync(AudioInputId audioInputId);

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

        // The user wishes the track to finish recording now, or at least when its quantized duration is reached.
        // Contractually requires State == NowSoundTrack_State.Recording.
        __declspec(dllexport) void NowSoundTrack_FinishRecording(TrackId trackId);

		// Get the current track frequency histogram; LPWSTR must actually reference a float buffer of the
		// same length as the outputBinCount argument passed to InitializeFFT, but must be typed as LPWSTR
		// and must have a capacity represented in two-byte wide characters (to match the P/Invoke style of
		// "pass in StringBuilder", known to work well).
		// Returns true if there was enough data to update the buffer, or false if there was not.
		__declspec(dllexport) void NowSoundTrack_GetFrequencies(TrackId trackId, void* floatBuffer, int floatBufferCapacity);

		// True if this is muted.
        // 
        // Note that something can be in FinishRecording state but still be muted, if the user is fast!
        // Hence this is a separate flag, not represented as a NowSoundTrack_State.
        __declspec(dllexport) bool NowSoundTrack_IsMuted(TrackId trackId);
        __declspec(dllexport) void NowSoundTrack_SetIsMuted(TrackId trackId, bool isMuted);

        // Delete this Track; after this, all methods become invalid to call (contract failure).
        void __declspec(dllexport) NowSoundTrack_Delete(TrackId trackId);
    };
}
