#pragma once

#include "pch.h"

#include <future>
#include <vector>

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
	class NowSoundGraph;

	// A single audio input and its associated objects.
	class NowSoundInput
	{
	private:
		// The NowSoundAudioGraph that created this input.
		const NowSoundGraph* _nowSoundGraph;

		// The audio input ID.
		const AudioInputId _audioInputId;

		// The input device.
		winrt::Windows::Media::Audio::AudioDeviceInputNode _inputDevice;

		// The frame output node which allows buffering input audio into memory.
		winrt::Windows::Media::Audio::AudioFrameOutputNode _frameOutputNode;

		// Vector of active Recorders; these are non-owning pointers borrowed from the collection of Tracks
		// held by NowSoundTrackAPI.
		std::vector<IRecorder<AudioSample, float>*> _recorders;

		// Mutex to prevent collision on the _recorders vector between creating a new track and handling the
		// existing recorder collection.
		std::mutex _recorderMutex;

		// Stream that buffers the last second of input audio, for latency compensation.
		// (Not really clear why latency compensation should be needed for NowSoundApp which shouldn't really
		// have any problematic latency... but this was needed for gesture latency compensation with Kinect,
		// so let's at least experiment with it.)
		BufferedSliceStream<AudioSample, float> _incomingAudioStream;

		// Adapter to record incoming data into _incomingAudioStream.
		StreamRecorder<AudioSample, float> _incomingAudioStreamRecorder;

		// Histograms on the input device volume -- one per channel (so stereo input gets two histograms).
		std::vector<std::unique_ptr<Histogram>> _incomingAudioHistograms;

	public:
		// Construct a NowSoundInput.  TODO: allow specifying which device.
		NowSoundInput(
			NowSoundGraph* audioGraph,
			AudioInputId audioInputId,
			winrt::Windows::Media::Audio::AudioDeviceInputNode inputNode,
			BufferAllocator<float>* audioAllocator);

		// Initialize this NowSoundInput by actually creating the requisite device nodes (an async operation).
		winrt::Windows::Foundation::IAsyncAction InitializeAsync();

		// Get information about this input.
		NowSoundInputInfo Info();

		// Create a recording track monitoring this input.
		void CreateRecordingTrack(TrackId id);

		// Handle any audio incoming for this input.
		void HandleIncomingAudio();
	};

}