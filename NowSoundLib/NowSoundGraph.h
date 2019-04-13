// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"

#include <future>
#include <vector>
#include <string>

#include "stdint.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "Histogram.h"
#include "NowSoundLibTypes.h"
#include "rosetta_fft.h"
#include "SliceStream.h"

#include "JuceHeader.h"

namespace NowSound
{
    class NowSoundInputAudioProcessor;

    // A single graph implementing the NowSoundGraphAPI operations.
    class NowSoundGraph
    {
    public: // API methods called by the NowSoundGraphAPI P/Invoke bridge methods

        // Initialize the audio graph subsystem.
        // Graph must be Uninitialized.  On completion, graph becomes Initialized.
        void Initialize();

        // Get the current state of the audio graph; intended to be efficiently pollable by the client.
        // This is one of the only two methods that may be called in any state whatoever.
        // All other methods declare which state the graph must be in to call the method, and the state
        // the method transitions the graph to once the asynchronous action is complete.
        // TODO: consider having some separate mutual exclusion to prevent multiple concurrent methods
        // from firing (don't want the graph to, e.g., get started twice in a race).
        NowSoundGraphState State() const;

		// Get the graph info for the created graph.
		// Graph must be at least Initialized.
		NowSoundGraphInfo Info();

        // Information about the final mixed output signal.
        // Graph must be at least Initialized.
        NowSoundSignalInfo OutputSignalInfo();

		// Get the ID of the input device with the given index (from 0 to Info().InputDeviceCount-1).
		// JUCETODO: void InputDeviceId(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity);

		// Get the name of the input device with the given index (from 0 to Info().InputDeviceCount-1).
		// JUCETODO: void InputDeviceName(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity);

		// Initialize given input device.  One mono input will be created per channel of the device.
		// This must be called only in Initialized state (for now; could relax this later perhaps).
		// JUCETODO: void InitializeDeviceInputs(int deviceIndex);

		// Initialize the FFT bins and other state.
		void InitializeFFT(
			int outputBinCount,
			float centralFrequency,
			int octaveDivisions,
			int centralBinIndex,
			int fftSize);
			
		// Info about the current graph time.
		// Graph must be Created or Running.
		NowSoundTimeInfo TimeInfo();

		// Info about the given input.
		// Graph must be Created or Running.
		NowSoundInputInfo InputInfo(AudioInputId inputId);

        // Play a user-selected sound file.
        // Graph must be Started.
        void PlayUserSelectedSoundFileAsync();

        // Create a new track and begin recording.
        // Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
		TrackId CreateRecordingTrackAsync(AudioInputId inputIndex);

	private: // Constructor and internal implementations

        // construct a graph, but do not yet initialize it
        NowSoundGraph();

        // Async helper method, to work around compiler bug with lambdas which await and capture this.
        void PlayUserSelectedSoundFileAsyncImpl();

        // Check that the expected state is the current state, and that no current state change is happening;
        // then mark that a state change is now happening.
        void PrepareToChangeState(NowSoundGraphState expectedState);

        // Check that a state change is happening, then switch the state to newState and mark the state change
        // as no longer happening.
        void ChangeState(NowSoundGraphState newState);

        // Set minimum buffer size in the device manager.
        void setBufferSize();

    private: // instance variables

        // The singleton (for now) graph; created by Initialize(), destroyed by Shutdown().
        static ::std::unique_ptr<NowSoundGraph> s_instance;

		// The AudioDeviceManager held by this Graph.
		// This is conceptually a singleton (just as the NowSoundGraph is), but we scope it within this type.
		juce::AudioDeviceManager _audioDeviceManager;

		// Callback object which couples the device manager to the audio processor graph.
		juce::AudioProcessorPlayer _audioProcessorPlayer;

		// The audio processor graph.
		juce::AudioProcessorGraph _audioProcessorGraph;

        // Ptr to the input node.
        juce::AudioProcessorGraph::Node::Ptr _audioInputNodePtr;

        // Ptr to the final mix node.
        juce::AudioProcessorGraph::Node::Ptr _audioOutputMixNodePtr;

        // Ptr to the actual output node.
        juce::AudioProcessorGraph::Node::Ptr _audioOutputNodePtr;

        // Information about the output signal.
        // Atomically updated using the associated mutex.
        NowSoundSignalInfo _outputSignalInfo;

        // Mutex guarding _outputSignalInfo, which may be polled from the UI thread while being
        // updated on the audio thread.
        std::mutex _outputSignalMutex;

        // Is this graph changing state? (Prevent re-entrant state changing methods.)
        bool _changingState;

        // The state of this graph.
        NowSoundGraphState _audioGraphState;

        // First, an allocator for 128-second 48Khz stereo float sample buffers.
        std::unique_ptr<BufferAllocator<float>> _audioAllocator;

        // The next TrackId to be allocated.
        TrackId _trackId;

		// The next AudioInputId to be allocated.
		AudioInputId _nextAudioInputId;

		// The vector of frequency bins.
		::std::vector<RosettaFFT::FrequencyBinBounds> _fftBinBounds;

		// The FFT size.
		int _fftSize;

		// The audio inputs we have; currently unchanging after graph creation.
        // Note that the processors held by these Ptrs are NowSoundInputAudioProcessors.
		// TODO: vaguely consider supporting dynamically added/removed inputs.
		std::vector<juce::AudioProcessorGraph::Node::Ptr> _audioInputs;

        // Mutex for the state of the graph.
        // The combination of _audioGraphState and _changingState must be updated atomically, or hazards are possible.
        std::mutex _stateMutex;

    public: // Implementation methods used from elsewhere in the library

        // The static instance of the graph.  We may eventually have multiple.
        static NowSoundGraph* Instance();

		// Create the singleton graph instance and initialize it.
		static void InitializeInstance();

		// Shut down the singleton graph instance and release it.
		static void ShutdownInstance();

        // These methods are for "internal" use only (since they not dllexported and are not using exportable types).

        // The (currently singleton) AudioGraph.
        // winrt::Windows::Media::Audio::AudioGraph GetAudioGraph() const;

        // The default audio output node.  TODO: support device selection.
        // winrt::Windows::Media::Audio::AudioDeviceOutputNode AudioDeviceOutputNode() const;

        // Audio allocator has static lifetime currently, but we give borrowed pointers rather than just statically
        // referencing it everywhere, because all this mutable static state continues to be concerning.
        BufferAllocator<float>* AudioAllocator() const;

		// Create an input device for the specified channel.
		void CreateInputDeviceForChannel(int channel);

		// Access the vector of frequency bins, when generating frequency histograms.
		const std::vector<RosettaFFT::FrequencyBinBounds>* BinBounds() const;

		// Access to the FFT size.
		int FftSize() const;

        // Access to the audio graph for node instantiation.
        juce::AudioProcessorGraph& JuceGraph();

        // Get a reference on one of the NowSoundInputs.
        NowSoundInputAudioProcessor* Input(AudioInputId audioInputId);

        // Add the connections of a SpatialAudioProcessor node.
        // This entails connecting the given inputChannel to newSpatialNode's channel 0,
        // and connecting all newSpatialNode's output channels to the graph's output channels.
        // (Note that this does not modify this NowSoundGraph's collection of Inputs or of Tracks; 
        // this just makes the JUCE graph connections.)
        // For now this always sets up one input connection and two output connections.
        void AddNodeToJuceGraph(juce::AudioProcessorGraph::Node::Ptr newSpatialNode, int inputChannel);
    };
}
