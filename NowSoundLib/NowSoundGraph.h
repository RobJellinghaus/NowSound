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

using namespace std::chrono;
using namespace winrt;

using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Render;
using namespace Windows::System;

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
        IAsyncAction InitializeAsyncImpl();

        // Async helper method, to work around compiler bug with lambdas which await and capture this.
        IAsyncAction CreateAudioGraphAsyncImpl();

        // Async helper method, to work around compiler bug with lambdas which await and capture this.
        IAsyncAction PlayUserSelectedSoundFileAsyncImpl();

    private: // instance variables
        // The singleton (for now) graph.
        static std::unique_ptr<NowSoundGraph> s_instance;

        // Is this graph changing state? (Prevent re-entrant state changing methods.)
        bool _changingState;

        // The AudioGraph managed by this NowSoundGraph.
        AudioGraph _audioGraph;

        // The state of this graph.
        NowSoundGraphState _audioGraphState;

        // The default output device. TODO: support multiple output devices.
        AudioDeviceOutputNode _deviceOutputNode;

        // First, an allocator for 128-second 48Khz stereo float sample buffers.
        BufferAllocator<float> _audioAllocator;

        // Audio frame, reused for copying audio.
        Windows::Media::AudioFrame _audioFrame;

        // The default input device. TODO: input device selection.
        AudioDeviceInputNode _defaultInputDevice;

        // This FrameOutputNode delivers the data from the (default) input device. TODO: support multiple input devices.
        AudioFrameOutputNode _inputDeviceFrameOutputNode;

        // The next TrackId to be allocated.
        TrackId _trackId;

        // Vector of active Recorders; these are non-owning pointers borrowed from the collection of Tracks
        // held by NowSoundTrackAPI.
        std::vector<IRecorder<AudioSample, float>*> _recorders;

        // Mutex for the collection of recorders; taken when adding to or traversing the recorder collection.
        std::mutex _recorderMutex;

    public: // Implementation methods used from elsewhere in the library
        // The static instance of the graph.  We may eventually have multiple.
        static NowSoundGraph* Instance();

        // These methods are for "internal" use only (since they not dllexported and are not using exportable types).

        // The (currently singleton) AudioGraph.
        Windows::Media::Audio::AudioGraph GetAudioGraph();

        // The default audio input node. TODO: support device selection.
        Windows::Media::Audio::AudioDeviceInputNode GetAudioDeviceInputNode();

        // The default audio output node.  TODO: support device selection.
        Windows::Media::Audio::AudioDeviceOutputNode GetAudioDeviceOutputNode();

        // The singleton (for reuse and to avoid reallocation) AudioFrame used for receiving input.
        // TODO: should this be per-track?
        // We keep a one-quarter-second (stereo float) AudioFrame and reuse it (between all inputs?! TODO fix this for multiple inputs)
        // This should probably be at least one second, but the currently hacked muting implementation simply stops populating output
        // buffers, which therefore still have time to drain.
        // TODO: restructure to use submixer and set output volume on submixer when muting/unmuting, to avoid this issue and allow more efficient bigger buffers here.
        Windows::Media::AudioFrame GetAudioFrame();

        // Audio allocator has static lifetime currently, but we give borrowed pointers rather than just statically
        // referencing it everywhere, because all this mutable static state continues to be concerning.
        BufferAllocator<float>* GetAudioAllocator();

        // A graph quantum has started; handle any available input audio.
        void HandleIncomingAudio();
    };

}
