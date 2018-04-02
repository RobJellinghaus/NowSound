// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

#include <future>
#include <vector>

#include "stdint.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "NowSoundLibTypes.h"
#include "Recorder.h"
#include "SliceStream.h"

namespace NowSound
{
    // A single graph implementing the NowSoundGraphAPI operations.
    class NowSoundGraph
    {
    public: // API methods
        // Get the current state of the audio graph; intended to be efficiently pollable by the client.
        // This is one of the only two methods that may be called in any state whatoever.
        // All other methods declare which state the graph must be in to call the method, and the state
        // the method transitions the graph to once the asynchronous action is complete.
        // TODO: consider having some separate mutual exclusion to prevent multiple concurrent methods
        // from firing (don't want the graph to, e.g., get started twice in a race).
        NowSoundGraphState GetGraphState();

        // If this returns true, the graph is currently in an asynchronous activity that is not yet complete.
        // The graph should be polled until this becomes false, at which time GetGraphState() will be updated.
        // All methods which change graph state have an implicit precondition that this method returns false.
        bool GetGraphChangingState();

        // Initialize the audio graph subsystem such that device information can be queried.
        // Graph must be Uninitialized.  On completion, graph becomes Initialized.
        void InitializeAsync();

        // Get the device info for the default render device.
        // Graph must not be Uninitialized or InError.
        NowSoundDeviceInfo GetDefaultRenderDeviceInfo();

        // Create the audio graph.
        // Graph must be Initialized.  On completion, graph becomes Created.
        void CreateAudioGraphAsync();

        // Get the graph info for the created graph.
        // Graph must be Created or Running.
        NowSoundGraphInfo GetGraphInfo();

        // Start the audio graph.
        // Graph must be Created.  On completion, graph becomes Started.
        void StartAudioGraphAsync();

        // Get the time of the created graph.
        // Graph must be Running.
        NowSoundTimeInfo GetTimeInfo();

        // Play a user-selected sound file.
        // Graph must be Started.
        void PlayUserSelectedSoundFileAsync();

        // Tear down the whole graph.
        // Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
        void DestroyAudioGraphAsync();

        // Create a new track and begin recording.
        // Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
        int CreateRecordingTrackAsync();

    private: // constructor and internal implementations
        // construct a graph, but do not yet initialize it
        NowSoundGraph();

        // Async helper method, to work around compiler bug with lambdas which await and capture this.
        winrt::Windows::Foundation::IAsyncAction InitializeAsyncImpl();

        // Async helper method, to work around compiler bug with lambdas which await and capture this.
        winrt::Windows::Foundation::IAsyncAction CreateAudioGraphAsyncImpl();

        // Async helper method, to work around compiler bug with lambdas which await and capture this.
        winrt::Windows::Foundation::IAsyncAction PlayUserSelectedSoundFileAsyncImpl();

        // Check that the expected state is the current state, and that no current state change is happening;
        // then mark that a state change is now happening.
        void PrepareToChangeState(NowSoundGraphState expectedState);

        // Check that a state change is happening, then switch the state to newState and mark the state change
        // as no longer happening.
        void ChangeState(NowSoundGraphState newState);

    private: // instance variables
        // The singleton (for now) graph.
        static std::unique_ptr<NowSoundGraph> s_instance;

        // Is this graph changing state? (Prevent re-entrant state changing methods.)
        bool _changingState;

        // The AudioGraph managed by this NowSoundGraph.
        winrt::Windows::Media::Audio::AudioGraph _audioGraph;

        // The state of this graph.
        NowSoundGraphState _audioGraphState;

        // The default output device. TODO: support multiple output devices.
        winrt::Windows::Media::Audio::AudioDeviceOutputNode _deviceOutputNode;

        // First, an allocator for 128-second 48Khz stereo float sample buffers.
        BufferAllocator<float> _audioAllocator;

        // Audio frame, reused for copying audio.
        winrt::Windows::Media::AudioFrame _audioFrame;

        // The default input device. TODO: input device selection.
        winrt::Windows::Media::Audio::AudioDeviceInputNode _defaultInputDevice;

        // This FrameOutputNode delivers the data from the (default) input device. TODO: support multiple input devices.
        winrt::Windows::Media::Audio::AudioFrameOutputNode _inputDeviceFrameOutputNode;

        // The next TrackId to be allocated.
        TrackId _trackId;

        // Vector of active Recorders; these are non-owning pointers borrowed from the collection of Tracks
        // held by NowSoundTrackAPI.
        std::vector<IRecorder<AudioSample, float>*> _recorders;

        // Mutex for the collection of recorders; taken when adding to or traversing the recorder collection.
        std::mutex _recorderMutex;

        // Mutex for the state of the graph.
        // The combination of _audioGraphState and _changingState must be updated atomically, or hazards are possible.
        std::mutex _stateMutex;

        // Stream that buffers the last second of input audio, for latency compensation.
        // (Not really clear why latency compensation should be needed for NowSoundApp which shouldn't really
        // have any problematic latency... but this was needed for gesture latency compensation with Kinect,
        // so let's at least experiment with it.)
        BufferedSliceStream<AudioSample, float> _incomingAudioStream;

        // Adapter to record incoming data into _incomingAudioStream.
        StreamRecorder<AudioSample, float> _incomingAudioStreamRecorder;

    public: // Implementation methods used from elsewhere in the library
        // The static instance of the graph.  We may eventually have multiple.
        static NowSoundGraph* Instance();

        // These methods are for "internal" use only (since they not dllexported and are not using exportable types).

        // Get the shared audio frame.
        winrt::Windows::Media::AudioFrame GetAudioFrame();

        // The (currently singleton) AudioGraph.
        winrt::Windows::Media::Audio::AudioGraph GetAudioGraph();

        // The default audio output node.  TODO: support device selection.
        winrt::Windows::Media::Audio::AudioDeviceOutputNode GetAudioDeviceOutputNode();

        // Audio allocator has static lifetime currently, but we give borrowed pointers rather than just statically
        // referencing it everywhere, because all this mutable static state continues to be concerning.
        BufferAllocator<float>* GetAudioAllocator();

        // A graph quantum has started; handle any available input audio.
        void HandleIncomingAudio();
    };

}
