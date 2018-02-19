// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

#include <future>

#include "stdint.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "NowSoundLibTypes.h"
#include "NowSoundTrack.h"
#include "SliceStream.h"

using namespace std::chrono;
using namespace winrt;

using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Render;
using namespace Windows::System;

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
        // There is a single "static" audio graph defined here; multiple audio graphs are not supported.
        // All async methods document the state the graph must be in when called, and the state the graph
        // transitions to on completion.
        // TODO: make this support multiple (non-static) graphs.
        class NowSoundGraph
        {
        public:
            // Get the current state of the audio graph; intended to be efficiently pollable by the client.
            // This is the only method that may be called in any state whatoever.
            static __declspec(dllexport) NowSoundGraph_State NowSoundGraph_GetGraphState();

            // Initialize the audio graph subsystem such that device information can be queried.
            // Graph must be Uninitialized.  On completion, graph becomes Initialized.
            static __declspec(dllexport) void NowSoundGraph_InitializeAsync();

            // Get the device info for the default render device.
            // Graph must not be Uninitialized or InError.
            static __declspec(dllexport) NowSound_DeviceInfo NowSoundGraph_GetDefaultRenderDeviceInfo();

            // Create the audio graph.
            // Graph must be Initialized.  On completion, graph becomes Created.
            static __declspec(dllexport) void NowSoundGraph_CreateAudioGraphAsync();

            // Get the graph info for the created graph.
            // Graph must be Created or Running.
            static __declspec(dllexport) NowSound_GraphInfo NowSoundGraph_GetGraphInfo();

            // Start the audio graph.
            // Graph must be Created.  On completion, graph becomes Started.
            static __declspec(dllexport) void NowSoundGraph_StartAudioGraphAsync();

            // Play a user-selected sound file.
            // Graph must be Started.
            static __declspec(dllexport) void NowSoundGraph_PlayUserSelectedSoundFileAsync();

            // Tear down the whole graph.
            // Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
            static __declspec(dllexport) void NowSoundGraph_DestroyAudioGraphAsync();

            // Create a new track and begin recording.
            // Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
            static __declspec(dllexport) TrackId NowSoundGraph_CreateRecordingTrackAsync();

        private:
            // Async helper method.
            static IAsyncAction PlayUserSelectedSoundFileAsyncImpl();

        public:
            // These methods are for internal use only (since they not dllexported and are not using exportable types).

            static Windows::Media::Audio::AudioGraph GetAudioGraph();
            static Windows::Media::Audio::AudioDeviceOutputNode GetAudioDeviceOutputNode();
            // Audio allocator has static lifetime currently, but we give borrowed pointers rather than just statically
            // referencing it everywhere, because all this mutable static state continues to be concerning.
            static BufferAllocator<float>* GetAudioAllocator();
        };

        // Interface used to invoke operations on a particular audio track.
        class NowSoundTrackAPI
        {
        public:
            // In what state is this track?
            static __declspec(dllexport) NowSoundTrack_State NowSoundTrack_State(TrackId trackId);

            // Duration in beats of current Clock.
            // Note that this is discrete (not fractional). This doesn't yet support non-beat-quantization.
            static __declspec(dllexport) int64_t /*Duration<Beat>*/ NowSoundTrack_BeatDuration(TrackId trackId);

            // What beat position is playing right now?
            // This uses Clock.Instance.Now to determine the current time, and is continuous because we may be
            // playing a fraction of a beat right now.  It will always be strictly less than BeatDuration.
            static __declspec(dllexport) float /*ContinuousDuration<Beat>*/ NowSoundTrack_BeatPositionUnityNow(TrackId trackId);

            // How long is this track, in samples?
            // This is increased during recording.  It may in general have fractional numbers of samples if 
            // Clock.Instance.BeatsPerMinute does not evenly divide Clock.Instance.SampleRateHz.
            static __declspec(dllexport) float /*ContinuousDuration<AudioSample>*/ NowSoundTrack_ExactDuration(TrackId trackId);

            // The starting moment at which this Track was created.
            static __declspec(dllexport) int64_t /*Time<AudioSample>*/ NowSoundTrack_StartTime(TrackId trackId);

            // The user wishes the track to finish recording now.
            // Contractually requires State == NowSoundTrack_State.Recording.
            static __declspec(dllexport) void NowSoundTrack_FinishRecording(TrackId trackId);

            // True if this is muted.
            // 
            // Note that something can be in FinishRecording state but still be muted, if the user is fast!
            // Hence this is a separate flag, not represented as a NowSoundTrack_State.
            static __declspec(dllexport) bool NowSoundTrack_IsMuted(TrackId trackId);
            static __declspec(dllexport) void NowSoundTrack_SetIsMuted(TrackId trackId, bool isMuted);

            // Delete this Track; after this, all methods become invalid to call (contract failure).
            static void __declspec(dllexport) NowSoundTrack_Delete(TrackId trackId);

            // TODO: Hack? Update the track to increment, e.g., its duration. (Should perhaps instead be computed whenever BeatDuration is queried???)
            static void __declspec(dllexport) NowSoundTrack_UnityUpdate(TrackId trackId);

        private:
            static std::vector<std::unique_ptr<NowSoundTrack>> _tracks;
            static NowSoundTrack* Track(TrackId id);
        };
    };
}
