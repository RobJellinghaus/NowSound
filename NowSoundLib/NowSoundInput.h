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
#include "Option.h"
#include "Recorder.h"
#include "SliceStream.h"

namespace NowSound
{
	class NowSoundGraph;

	// A single mono audio input and its associated objects.
	class NowSoundInput
	{
	private:
		// The NowSoundAudioGraph that created this input.
		const NowSoundGraph* _nowSoundGraph;

		// The audio input ID of this input.
		const AudioInputId _audioInputId;

		// The input device. Note that this device input node may be shared between multiple NowSoundInputs.
		winrt::Windows::Media::Audio::AudioDeviceInputNode _inputDevice;

		// The channel to select from the input device.
		int _channel;

		// The panning value (0 = left, 1 = right).
		float _pan;

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

		// Histograms on the input volume of this input's channel.
		Histogram _volumeHistogram;

		// Temporary buffer, reused across calls to HandleInputAudio.
		std::vector<float> _monoBuffer;

	public:
		// Construct a NowSoundInput.
		NowSoundInput(
			NowSoundGraph* audioGraph,
			AudioInputId audioInputId,
			winrt::Windows::Media::Audio::AudioDeviceInputNode inputNode,
			BufferAllocator<float>* audioAllocator,
			int channel);

		// Set the stereo panning (0 = left, 1 = right, 0.5 = center).
		// TODO: implement panning fully and expose to user!
		void Pan(float pan);

		// Get information about this input.
		NowSoundInputInfo Info();

		// Create a recording track monitoring this input.
		// Note that this is not concurrency-safe with respect to other calls to this method.
		// (It is of course concurrency-safe with respect to ongoing audio activity.)
		void CreateRecordingTrack(TrackId id);

		// Handle any audio incoming for this input.
		// This method is invoked by audio quantum processing, as an audio activity.
		void HandleIncomingAudio();
	};
}
