// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

#include <future>
#include <vector>
#include <string>

#include "BufferAllocator.h"
#include "Check.h"
#include "Histogram.h"
#include "NowSoundInput.h"
#include "NowSoundLibTypes.h"
#include "Recorder.h"
#include "rosetta_fft.h"
#include "SliceStream.h"

namespace NowSound
{
	// NB: All states >= DeviceStateInitialized will allow some methods
	// to be called successfully on the Audio Client
	enum class DeviceState
	{
		DeviceStateUnInitialized,
		DeviceStateInError,
		DeviceStateDiscontinuity,
		DeviceStateFlushing,
		DeviceStateActivated,
		DeviceStateInitialized,
		DeviceStateStarting,
		DeviceStatePlaying,
		DeviceStateCapturing,
		DeviceStatePausing,
		DeviceStatePaused,
		DeviceStateStopping,
		DeviceStateStopped
	};
	
	struct DEVICEPROPS
	{
		bool IsHWOffload;
		bool IsTonePlayback;
		bool IsBackground;
		bool IsRawSupported;
		bool IsRawChosen;
		bool IsLowLatency;
		REFERENCE_TIME hnsBufferDuration;
		int32_t Frequency;
		// IRandomAccessStream^    ContentStream;
	};

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
        NowSoundGraphState State() const;

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

		// Initialize given input device.  One mono input will be created per channel of the device.
		// This must be called only in Initialized state (for now; could relax this later perhaps).
		void InitializeDeviceInputs(int deviceIndex);

		// Initialize the FFT bins and other state.
		void InitializeFFT(
			int outputBinCount,
			float centralFrequency,
			int octaveDivisions,
			int centralBinIndex,
			int fftSize);
			
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
        void PlayUserSelectedSoundFileAsyncImpl();

        // Check that the expected state is the current state, and that no current state change is happening;
        // then mark that a state change is now happening.
        void PrepareToChangeState(NowSoundGraphState expectedState);

        // Check that a state change is happening, then switch the state to newState and mark the state change
        // as no longer happening.
        void ChangeState(NowSoundGraphState newState);

		// Continuation method after ActivateAudioInterfaceAsync.
		void ContinueActivation(IActivateAudioInterfaceAsyncOperation *operation);

		// COM trampoline.
		struct NowSoundGraphActivationHandler
			: winrt::implements<NowSoundGraphActivationHandler, IActivateAudioInterfaceCompletionHandler>
		{
			NowSoundGraph* _graph;
			NowSoundGraphActivationHandler(NowSoundGraph* graph) : _graph(graph) {}
			HRESULT ActivateCompleted(IActivateAudioInterfaceAsyncOperation *operation);
		};

    private: // instance variables

        // The singleton (for now) graph.
        static ::std::unique_ptr<NowSoundGraph> s_instance;

        // Is this graph changing state? (Prevent re-entrant state changing methods.)
        bool _changingState;

        // The AudioGraph managed by this NowSoundGraph.
        // winrt::Windows::Media::Audio::AudioGraph _audioGraph;

        // The state of this graph.
        NowSoundGraphState _audioGraphState;

		// COM trampoline instance for ActivateAudioInterfaceAsync continuation.
		NowSoundGraphActivationHandler _handler;

		// The device string of the output device.
		std::wstring _deviceIdString;

		int32_t _bufferFrames;
		HANDLE _sampleReadyEvent;
		MFWORKITEM_KEY _sampleReadyKey;
		std::mutex _sampleMutex;

		WAVEFORMATEX _audioClientFormat;

		uint32_t _defaultPeriodInFrames;
		uint32_t _fundamentalPeriodInFrames;
		uint32_t _maxPeriodInFrames;
		uint32_t _minPeriodInFrames;

		winrt::com_ptr<IAudioClient3> _audioClient;
		winrt::com_ptr<IAudioRenderClient> _audioRenderClient;
		winrt::com_ptr<IMFAsyncResult> _sampleReadyAsyncResult;

		DEVICEPROPS _deviceProps;

        // The default output device. TODO: support multiple output devices.
        // winrt::Windows::Media::Audio::AudioDeviceOutputNode _deviceOutputNode;

		// The AudioGraph DeviceInformation structures for all input devices.
		// ::std::vector<winrt::Windows::Devices::Enumeration::DeviceInformation> _inputDeviceInfos;

        // First, an allocator for 128-second 48Khz stereo float sample buffers.
        std::unique_ptr<BufferAllocator<float>> _audioAllocator;

        // The next TrackId to be allocated.
        TrackId _trackId;

		// The next AudioInputId to be allocated.
		AudioInputId _nextAudioInputId;

		// The audio device indices to initialize.
		::std::vector<int> _inputDeviceIndicesToInitialize;

		// The vector of frequency bins.
		::std::vector<RosettaFFT::FrequencyBinBounds> _fftBinBounds;

		// The FFT size.
		int _fftSize;

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
        // winrt::Windows::Media::Audio::AudioGraph GetAudioGraph() const;

        // The default audio output node.  TODO: support device selection.
        // winrt::Windows::Media::Audio::AudioDeviceOutputNode AudioDeviceOutputNode() const;

        // Audio allocator has static lifetime currently, but we give borrowed pointers rather than just statically
        // referencing it everywhere, because all this mutable static state continues to be concerning.
        BufferAllocator<float>* AudioAllocator() const;

		// Create an input device (or a pair of them, if monoPair is true).
		void CreateInputDeviceAsync(int deviceIndex);

		// Create an input device (or a pair of them, if monoPair is true).
		// void CreateInputDeviceFromNode(winrt::Windows::Media::Audio::AudioDeviceInputNode deviceInputNode, int channel);

		// A graph quantum has started; handle any available input audio.
        void HandleIncomingAudio();

		// Access the vector of frequency bins, when generating frequency histograms.
		const std::vector<RosettaFFT::FrequencyBinBounds>* BinBounds() const;

		// Access to the FFT size.
		int FftSize() const;
    };
}
