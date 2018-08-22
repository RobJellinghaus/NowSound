// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include <algorithm>

#include "Clock.h"
#include "GetBuffer.h"
#include "Histogram.h"
#include "MagicConstants.h"
#include "NowSoundLib.h"
#include "NowSoundGraph.h"
#include "NowSoundTrack.h"

using namespace std;
using namespace winrt;

using namespace Windows::Foundation;
using namespace Windows::Media;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Render;

namespace NowSound
{
	NowSoundInput::NowSoundInput(
		NowSoundGraph* nowSoundGraph,
		AudioInputId inputId,
		AudioDeviceInputNode inputNode,
		BufferAllocator<float>* audioAllocator,
		int channel)
		: _nowSoundGraph{ nowSoundGraph },
		_audioInputId{ inputId },
		_inputDevice{ inputNode },
		_channel{ channel },
		_pan{ 0.5 },
		// TODO: make NowSoundInputs on the same device share FrameOutputNodes as well as DeviceInputNodes
		_frameOutputNode{ nowSoundGraph->GetAudioGraph().CreateFrameOutputNode() },
		_recorders{},
		_incomingAudioStream{ 0, Clock::Instance().ChannelCount(), audioAllocator, Clock::Instance().SampleRateHz(), /*useExactLoopingMapper:*/false },
		_incomingAudioStreamRecorder{ &_incomingAudioStream },
		_volumeHistogram{ (int)Clock::Instance().TimeToSamples(MagicConstants::RecentVolumeDuration).Value() }
	{
		_inputDevice.AddOutgoingConnection(_frameOutputNode);

		// TODO: figure out how to not have this be a HORRIBLE HORRIBLE HACK
		if (inputId == AudioInput1)
		{
			// only add ONE connection from input device to output node
			// TODO: figure out how in the heck sound effects will work with this
			_inputDevice.AddOutgoingConnection(nowSoundGraph->AudioDeviceOutputNode());
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

	void NowSoundInput::HandleIncomingAudio()
	{
		AudioFrame frame = _frameOutputNode.GetFrame();

		// The frame has this many channels.
		// TODO: decouple input channels from overall graph channels.
		int channelCount = Clock::Instance().ChannelCount();

		uint8_t* dataInBytes{};
		uint32_t capacityInBytes{};

		// OMG KENNY KERR WINS AGAIN:
		// https://gist.github.com/kennykerr/f1d941c2d26227abbf762481bcbd84d3
		Windows::Media::AudioBuffer buffer(frame.LockBuffer(Windows::Media::AudioBufferAccessMode::Read));
		IMemoryBufferReference reference(buffer.CreateReference());
		winrt::impl::com_ref<IMemoryBufferByteAccess> interop = reference.as<IMemoryBufferByteAccess>();
		check_hresult(interop->GetBuffer(&dataInBytes, &capacityInBytes));

		if (capacityInBytes == 0)
		{
			// we don't count zero-byte frames... and why do they ever happen???
			return;
		}

		// Must be multiple of channels * sizeof(float)
		uint32_t sampleSizeInBytes = Clock::Instance().ChannelCount() * sizeof(float);
		// make sure capacity is an exact multiple of sample size
		Check((capacityInBytes & (sampleSizeInBytes - 1)) == 0);

		uint32_t bufferStart = 0;
		if (Clock::Instance().Now().Value() == 0)
		{
			// if maxCapacityEncountered is greater than the audio graph buffer size, 
			// then the audio graph decided to give us a big backload of buffer content
			// as its first callback.  Not sure why it does this, but we don't want it,
			// so take only the tail of the buffer.
			uint32_t latencyInSamples = (uint32_t)_nowSoundGraph->GetAudioGraph().LatencyInSamples();

			if (latencyInSamples == 0)
			{
				// sorry audiograph, don't really believe you when you say zero latency.
				latencyInSamples = MagicConstants::AudioFrameDuration.Value();
			}

			uint32_t latencyBufferSize = (uint32_t)latencyInSamples * sampleSizeInBytes;
			if (capacityInBytes > latencyBufferSize)
			{
				bufferStart = capacityInBytes - latencyBufferSize;
				capacityInBytes = latencyBufferSize;
			}
		}

		Duration<AudioSample> duration(capacityInBytes / sampleSizeInBytes);

		// update input volume histograms
		float* dataInFloats = (float*)(dataInBytes + bufferStart);

		// Make sure our buffer is big enough; note that we don't multiply by channelCount because this is a mono buffer.
		_monoBuffer.resize(duration.Value());

		// Copy the data from the appropriate channel of the input device into the mono buffer.
		for (int i = 0 ; i < duration.Value(); i++)
		{
			float value = dataInFloats[i * channelCount + _channel];
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
				bool stillRecording = recorder->Record(duration, _monoBuffer.data());

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
