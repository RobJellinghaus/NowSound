// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

#include <future>
#include <vector>
#include <string>

#include "stdint.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "Histogram.h"
#include "NowSoundInput.h"
#include "NowSoundLibTypes.h"
#include "Recorder.h"
#include "SliceStream.h"

namespace NowSound
{
    // A single graph implementing the NowSoundGraphAPI operations.
    class NowSoundGraph
    {
    public: // API methods called by the NowSoundGraphAPI P/Invoke bridge methods

        // Get the current state of the audio graph; intended to be efficiently pollable by the client.
        // This is one of the only two methods that may be called in any state whatoever.
        // All other methods declare which state the graph must be in to call the method, and the state
        // the method transitions the graph to once the asynchronous action is complete.
        // TODO: consider having some separate mutual exclusion to prevent multiple concurrent methods
        // from firing (don't want the graph to, e.g., get started twice in a race).
        NowSoundGraphState State();

        // Initialize the audio graph subsystem such that device information can be queried.
        // Graph must be Uninitialized.  On completion, graph becomes Initialized.
        void InitializeAsync();

		// Get the graph info for the created graph.
		// Graph must be at least Initialized.
		NowSoundGraphInfo Info();

		// Get the ID of the input device with the given index (from 0 to Info().InputDeviceCount-1).
		void InputDeviceId(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity);

		// Get the name of the input device with the given index (from 0 to Info().InputDeviceCount-1).
		void InputDeviceName(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity);

		// Initialize given input; return its newly assigned input ID. (AudioInputIds only apply to created devices.)
		// If monoPair, then two AudioInputs will be created; the ID of the first will be returned, the ID of the second is one greater.
		// This must be called only in Initialized state (for now; could relax this later perhaps).
		AudioInputId InitializeInputDevice(int deviceIndex, bool monoPair);

		// Create the audio graph.
		// Graph must be Initialized.  On completion, graph becomes Created.
		void CreateAudioGraphAsync();

		// Info about the current graph time.
		// Graph must be Created or Running.
		NowSoundTimeInfo TimeInfo();

		// Info about the given input.
		// Graph must be Created or Running.
		NowSoundInputInfo InputInfo(AudioInputId inputId);

        // Start the audio graph.
        // Graph must be Created.  On completion, graph becomes Running.
        void StartAudioGraphAsync();

        // Play a user-selected sound file.
        // Graph must be Started.
        void PlayUserSelectedSoundFileAsync();

        // Tear down the whole graph.
        // Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
        void DestroyAudioGraphAsync();

        // Create a new track and begin recording.
        // Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
		TrackId CreateRecordingTrackAsync(AudioInputId inputIndex);

    private: // Constructor and internal implementations

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
        static ::std::unique_ptr<NowSoundGraph> s_instance;

        // Is this graph changing state? (Prevent re-entrant state changing methods.)
        bool _changingState;

        // The AudioGraph managed by this NowSoundGraph.
        winrt::Windows::Media::Audio::AudioGraph _audioGraph;

        // The state of this graph.
        NowSoundGraphState _audioGraphState;

        // The default output device. TODO: support multiple output devices.
        winrt::Windows::Media::Audio::AudioDeviceOutputNode _deviceOutputNode;

		// The AudioGraph DeviceInformation structures for all input devices.
		::std::vector<winrt::Windows::Devices::Enumeration::DeviceInformation> _inputDeviceInfos;

        // First, an allocator for 128-second 48Khz stereo float sample buffers.
        std::unique_ptr<BufferAllocator<float>> _audioAllocator;

        // The next TrackId to be allocated.
        TrackId _trackId;

		// The next AudioInputId to be allocated.
		AudioInputId _nextAudioInputId;

		// The audio device indices to initialize.
		::std::vector<int> _inputDeviceIndicesToInitialize;

		// Which devices are actually to be created as mono pairs?
		::std::vector<bool> _inputDeviceIsMonoPair;

		// The audio inputs we have; currently unchanging after graph creation.
		// TODO: vaguely consider supporting dynamically added/removed inputs.
		std::vector<std::unique_ptr<NowSoundInput>> _audioInputs;

        // Mutex for the state of the graph.
        // The combination of _audioGraphState and _changingState must be updated atomically, or hazards are possible.
        std::mutex _stateMutex;

    public: // Implementation methods used from elsewhere in the library

        // The static instance of the graph.  We may eventually have multiple.
        static NowSoundGraph* Instance();

        // These methods are for "internal" use only (since they not dllexported and are not using exportable types).

#if STATIC_AUDIO_FRAME
        // Get the shared audio frame.
        winrt::Windows::Media::AudioFrame GetAudioFrame();
#endif

        // The (currently singleton) AudioGraph.
        winrt::Windows::Media::Audio::AudioGraph GetAudioGraph() const;

        // The default audio output node.  TODO: support device selection.
        winrt::Windows::Media::Audio::AudioDeviceOutputNode GetAudioDeviceOutputNode() const;

        // Audio allocator has static lifetime currently, but we give borrowed pointers rather than just statically
        // referencing it everywhere, because all this mutable static state continues to be concerning.
        BufferAllocator<float>* GetAudioAllocator();

		// Create an input device (or a pair of them, if monoPair is true).
		winrt::Windows::Foundation::IAsyncAction CreateInputDeviceAsync(int deviceIndex, bool monoPair);

		// Create an input device (or a pair of them, if monoPair is true).
		void CreateInputDeviceFromNode(winrt::Windows::Media::Audio::AudioDeviceInputNode deviceInputNode, Option<int> channelIndexOpt);

		// A graph quantum has started; handle any available input audio.
        void HandleIncomingAudio();
    };
}
