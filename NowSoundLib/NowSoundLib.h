// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "Check.h"
#include "SliceStream.h"
#include <future>

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
        // Information about an audio device.
        struct NowSound_DeviceInfo
        {
            LPWSTR Id;
            LPWSTR Name;

            // Construct a DeviceInfo; it will directly reference the given dictionary (no copying).
            // Note that this does *not* own the strings; these must be owned elsewhere.
            NowSound_DeviceInfo(LPWSTR id, LPWSTR name)
            {
                Id = id;
                Name = name;
            }
        };

        // Information about an audio graph.
        struct NowSound_GraphInfo
        {
            int32_t LatencyInSamples;
            int32_t SamplesPerQuantum;

            NowSound_GraphInfo(int32_t latencyInSamples, int32_t samplesPerQuantum)
            {
                LatencyInSamples = latencyInSamples;
                SamplesPerQuantum = samplesPerQuantum;
            }
        };

        enum NowSoundGraph_State
        {
            // InitializeAsync() has not yet been called.
            Uninitialized,

            // Some error has occurred; GetLastError() will have details.
            InError,

            // A gap in incoming audio data was detected; this should ideally never happen.
            // NOTYET: Discontinuity,

            // InitializeAsync() has completed; devices can now be queried.
            Initialized,

            // CreateAudioGraphAsync() has completed; other methods can now be called.
            Created,

            // The audio graph has been started and is running.
            Running,

            // The audio graph has been stopped.
            // NOTYET: Stopped,
        };

        // The ID of a NowSound track; avoids issues with marshaling object references.
        // Note that 0 is a valid value.
        // NOTYET: typedef size_t TrackId;

        // Operations on the audio graph as a whole.
        // There is a single "static" audio graph defined here; multiple audio graphs are not supported.
        // All async methods document the state the graph must be in when called, and the state the graph
        // transitions to on completion.
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
            static __declspec(dllexport) void NowSoundGraph_CreateAudioGraphAsync(NowSound_DeviceInfo outputDevice);

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
            static __declspec(dllexport) void __cdecl NowSoundGraph_DestroyAudioGraphAsync();

        private:
            static IAsyncAction PlayUserSelectedSoundFileAsyncImpl();
        };

        /*
        // 
        // The state of a particular IHolofunkAudioTrack.
        // 
        public enum TrackState
        {
            // 
            // The track is being recorded and it is not known when it will finish.
            // 
            Recording,

            // 
            // The track is finishing off its now-known recording time.
            // 
            FinishRecording,

            // 
            // The track is playing back, looping.
            // 
            Looping,
        }

        // 
        // Interface used by the Unity code to invoke operations on a particular audio track.
        // 
        // 
        // This interface exists because the HolofunkAudioGraph assembly is at a higher level (referentially) than
        // the Holofunk-Unity assembly. So the HolofunkAudioGraph assembly implements these operations, and the
        // Holofunk-Unity code invokes them via this "upcall" interface.
        // </remarks>
        public interface IHolofunkAudioTrack
        {
            // 
            // In what state is this track?
            // 
            TrackState State{ get; }

                // 
                // Duration in beats of current Clock.
                // 
                // 
                // Note that this is discrete (not fractional). This doesn't yet support non-beat-quantization.
                // </remarks>
            Duration<Beat> BeatDuration{ get; }

                // 
                // What beat position is playing right now?
                // 
                // 
                // This uses Clock.Instance.Now to determine the current time, and is continuous because we may be
                // playing a fraction of a beat right now.  It will always be strictly less than BeatDuration.
                // </remarks>
            ContinuousDuration<Beat> BeatPositionUnityNow{ get; }

                // 
                // How long is this track, in samples?
                // 
                // 
                // This is increased during recording.  It may in general have fractional numbers of samples if 
                // Clock.Instance.BeatsPerMinute does not evenly divide Clock.Instance.SampleRateHz.
                // </remarks>
            ContinuousDuration<AudioSample> Duration{ get; }

                // 
                // The starting moment at which this Track was created.
                // 
            Moment StartMoment{ get; }

                // 
                // The user wishes the track to finish recording now.
                // 
                // 
                // Contractually requires State == TrackState.Recording.
                // </remarks>
            void FinishRecording();

            // 
            // True if this is muted.
            // 
            // 
            // Note that something can be in FinishRecording state but still be muted, if the user is fast!
            // Hence this is a separate flag, not represented as a TrackState.
            // </remarks>
            bool IsMuted{ get; set; }

                // 
                // Delete this Track; after this, all methods become invalid to call (contract failure).
                // 
            void Delete();

            // 
            // TODO: Hack? Update the track to increment, e.g., its duration. (Should perhaps instead be computed whenever BeatDuration is queried???)
            // 
            void UnityUpdate();
        }
        */
    }
}
