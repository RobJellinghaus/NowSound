// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include <algorithm>

#include "Clock.h"
#include "GetBuffer.h"
#include "Histogram.h"
#include "MagicConstants.h"
#include "NowSoundLib.h"
#include "NowSoundGraph.h"
#include "NowSoundTrack.h"
#include "NowSoundInput.h"

using namespace std;
using namespace winrt;

using namespace winrt::Windows::Foundation;

namespace NowSound
{
	NowSoundInputAudioProcessor::NowSoundInputAudioProcessor(
		NowSoundGraph* nowSoundGraph,
		AudioInputId inputId,
		BufferAllocator<float>* audioAllocator,
		int channel)
		: _nowSoundGraph{ nowSoundGraph },
		_audioInputId{ inputId },
		_channel{ channel },
		_pan{ 0.5 },
		_incomingAudioStream{ 0, Clock::Instance().ChannelCount(), audioAllocator, Clock::Instance().SampleRateHz(), /*useExactLoopingMapper:*/false },
		_incomingAudioStreamRecorder{ &_incomingAudioStream },
		_volumeHistogram{ (int)Clock::Instance().TimeToSamples(MagicConstants::RecentVolumeDuration).Value() }
	{
	}

	NowSoundInputInfo NowSoundInputAudioProcessor::Info()
	{
		float volume = _volumeHistogram.Average();

		NowSoundInputInfo ret;
		ret.Volume = volume;
		ret.Pan = _pan;
		return ret;
	}

	void NowSoundInputAudioProcessor::CreateRecordingTrack(TrackId id)
	{
		juce::AudioProcessorGraph::Node::Ptr newTrackPtr = _nowSoundGraph->JuceGraph().addNode(
            new NowSoundTrackAudioProcessor(_nowSoundGraph, id, _audioInputId, _incomingAudioStream, _pan));

		// New tracks are created as recording; lock the _recorders collection and add this new track.
		// Move the new track over to the collection of tracks in NowSoundTrackAPI.
		NowSoundTrackAudioProcessor::AddTrack(id, newTrackPtr);
	}

	void NowSoundInputAudioProcessor::processBlock(juce::AudioBuffer<float>& audioBuffer, juce::MidiBuffer& midiBuffer)
	{
        int bufferCount = audioBuffer.getNumSamples();

		// Make sure our buffer is big enough; note that we don't multiply by channelCount because this is a mono buffer.
		_monoBuffer.resize(bufferCount);

		// Copy the data from the appropriate channel of the input device into the mono buffer.
		for (int i = 0 ; i < bufferCount; i++)
		{
			float value = buffer[i];
			_volumeHistogram.Add(std::abs(value));
			_monoBuffer.data()[i] = value;
		}

		// iterate through all active Recorders
		// note that Recorders must be added or removed only inside the audio graph
		// (e.g. QuantumStarted or FrameInputAvailable)
		std::vector<IRecorder<AudioSample, float>*> _completedRecorders{};
		{
			std::lock_guard<std::mutex> guard(_recorderMutex);

			// Give the new audio to each Recorder, collecting the ones that are done.
			for (IRecorder<AudioSample, float>* recorder : _recorders)
			{
				bool stillRecording = recorder->Record({ bufferCount }, _monoBuffer.data());

				if (!stillRecording)
				{
					_completedRecorders.push_back(recorder);
				}
			}

			// Now remove all the done ones.
			for (IRecorder<AudioSample, float>* completedRecorder : _completedRecorders)
			{
				// not optimally efficient but we will only ever have one or two completed per incoming audio frame
				_recorders.erase(std::find(_recorders.begin(), _recorders.end(), completedRecorder));
			}
		}
	}
}
