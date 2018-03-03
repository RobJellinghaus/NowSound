// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

#include <future>

#include "stdint.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "NowSoundLibTypes.h"
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
    public:
        // Get the current state of the audio graph; intended to be efficiently pollable by the client.
        // This is the only method that may be called in any state whatoever.
        NowSoundGraphState GetGraphState();

        // Initialize the audio graph subsystem such that device information can be queried.
        // Graph must be Uninitialized.  On completion, graph becomes Initialized.
        void InitializeAsync();

        // Get the device info for the default render device.
        // Graph must not be Uninitialized or InError.
        NowSound_DeviceInfo _GetDefaultRenderDeviceInfo();

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

    private:
        // construct a graph, but do not yet initialize it
        NowSoundGraph();

        // Async helper method.
        static IAsyncAction PlayUserSelectedSoundFileAsyncImpl();

        // The singleton (for now) graph.
        static std::unique_ptr<NowSoundGraph> s_instance;

        // The AudioGraph managed by this NowSoundGraph.
        AudioGraph _audioGraph;

        // The state of this graph.
        NowSoundGraphState _audioGraphState;

        // The default output device. TODO: support multiple output devices.
        AudioDeviceOutputNode _deviceOutputNode{ nullptr };

        // First, an allocator for 128-second 48Khz stereo float sample buffers.
        BufferAllocator<float> _audioAllocator;

        // Audio frame, reused for copying audio.
        AudioFrame _audioFrame{ nullptr };

        // The default input device. TODO: input device selection.
        AudioDeviceInputNode _defaultInputDevice{ nullptr };

        // This FrameOutputNode delivers the data from the (default) input device. TODO: support multiple input devices.
        AudioFrameOutputNode _inputDeviceFrameOutputNode{ nullptr };

        // The next TrackId to be allocated.
        TrackId _trackId{ 0 };

    public:
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
