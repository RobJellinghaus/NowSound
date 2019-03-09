#pragma once

#include "stdafx.h"

#include <future>
#include <vector>

#include "stdint.h"

#include "BaseAudioProcessor.h"
#include "BufferAllocator.h"
#include "Check.h"
#include "Histogram.h"
#include "NowSoundLibTypes.h"
#include "Option.h"
#include "Recorder.h"
#include "SliceStream.h"

#include "JuceHeader.h"

namespace NowSound
{
	class NowSoundGraph;

	// Container object that manages the current sound effects on a particular input, if any.
	class NowSoundInputAudioProcessor : public BaseAudioProcessor
	{
	private:
		// The NowSoundAudioGraph that created this input.
		NowSoundGraph* _nowSoundGraph;

		// The audio input ID of this input.
		const AudioInputId _audioInputId;

		// The channel to select from the input device.
		int _channel;

		// The panning value (0 = left, 1 = right).
		float _pan;

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
		NowSoundInputAudioProcessor(
			NowSoundGraph* audioGraph,
			AudioInputId audioInputId,
			BufferAllocator<float>* audioAllocator,
			int channel);

        virtual const String getName() const override { return L"NowSoundInputAudioProcessor"; }

        // Handle any audio incoming for this input.
        // This method is invoked by audio quantum processing, as an audio activity.
        virtual void processBlock(juce::AudioBuffer<float>& audioBuffer, juce::MidiBuffer& midiBuffer);
        
        // Set the stereo panning (0 = left, 1 = right, 0.5 = center).
		// TODO: implement panning fully and expose to user!
		void Pan(float pan);

		// Get information about this input.
		NowSoundInputInfo Info();

		// Create a recording track monitoring this input.
		// Note that this is not concurrency-safe with respect to other calls to this method.
		// (It is of course concurrency-safe with respect to ongoing audio activity.)
		void CreateRecordingTrack(TrackId id);
	};
}
