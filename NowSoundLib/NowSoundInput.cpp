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
    NowSoundInput::NowSoundDemux::NowSoundDemux(NowSoundGraph* graph)
	{
		_graph = graph;
	}

	const juce::String NowSoundInput::NowSoundDemux::getName() const {
		return L"NowSoundDemux";
	}

	void NowSoundInput::NowSoundDemux::processBlock(
		AudioBuffer<float>& buffer,
		MidiBuffer& midiMessages)
	{
		Check(_graph->Info().ChannelCount == buffer.getNumChannels());
		for (int i = 0; i < _graph->Info().ChannelCount; i++)
		{
			// copy however much data it is to that specific input and any recorders
			_graph->Inputs()[i].get()->HandleIncomingAudio(
				buffer.getArrayOfReadPointers()[i],
				buffer.getNumSamples());
		}
	}

	NowSoundInput::NowSoundInput(
		NowSoundGraph* nowSoundGraph,
		AudioInputId inputId,
		// JUCETODO: AudioDeviceInputNode inputNode,
		BufferAllocator<float>* audioAllocator,
		int channel)
		: _nowSoundGraph{ nowSoundGraph },
		_audioInputId{ inputId },
		// _inputDevice{ inputNode },
		_channel{ channel },
		_pan{ 0.5 },
		// TODO: make NowSoundInputs on the same device share FrameOutputNodes as well as DeviceInputNodes
		// _frameOutputNode{ nowSoundGraph->GetAudioGraph().CreateFrameOutputNode() },
		_recorders{},
		_incomingAudioStream{ 0, Clock::Instance().ChannelCount(), audioAllocator, Clock::Instance().SampleRateHz(), /*useExactLoopingMapper:*/false },
		_incomingAudioStreamRecorder{ &_incomingAudioStream },
		_volumeHistogram{ (int)Clock::Instance().TimeToSamples(MagicConstants::RecentVolumeDuration).Value() }
	{
		// _inputDevice.AddOutgoingConnection(_frameOutputNode);

		// TODO: figure out how to not have this be a HORRIBLE HORRIBLE HACK
		if (inputId == AudioInput1)
		{
			// only add ONE connection from input device to output node
			// TODO: figure out how in the heck sound effects will work with this
			// _inputDevice.AddOutgoingConnection(nowSoundGraph->AudioDeviceOutputNode());
		}
	}

	NowSoundInputInfo NowSoundInput::Info()
	{
		float volume = _volumeHistogram.Average();

		NowSoundInputInfo ret;
		ret.Volume = volume;
		ret.Pan = _pan;
		return ret;
	}

	void NowSoundInput::CreateRecordingTrack(TrackId id)
	{
		std::unique_ptr<NowSoundTrack> newTrack(new NowSoundTrack(_nowSoundGraph, id, _audioInputId, _incomingAudioStream, _pan));

		// New tracks are created as recording; lock the _recorders collection and add this new track.
		// Note that the _recorders collection holds a raw pointer, e.g. a weak reference, to the track.
		{
			std::lock_guard<std::mutex> guard(_recorderMutex);
			_recorders.push_back(newTrack.get());
		}

		// Move the new track over to the collection of tracks in NowSoundTrackAPI.
		NowSoundTrack::AddTrack(id, std::move(newTrack));
	}

	void NowSoundInput::HandleIncomingAudio(const float* buffer, int bufferCount)
	{
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
