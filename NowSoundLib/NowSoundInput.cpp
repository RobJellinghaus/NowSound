// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include <algorithm>

#include "Clock.h"
#include "GetBuffer.h"
#include "Histogram.h"
#include "MagicNumbers.h"
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
		Option<int> channelOpt)
		: _nowSoundGraph{ nowSoundGraph },
		_audioInputId{ inputId },
		_inputDevice{ inputNode },
		_channelOpt{ channelOpt },
		_pan{ 0.5 },
		_frameOutputNode{ nowSoundGraph->GetAudioGraph().CreateFrameOutputNode() },
		_recorders{},
		_incomingAudioStream{ 0, Clock::Instance().ChannelCount(), audioAllocator, Clock::Instance().SampleRateHz(), /*useExactLoopingMapper:*/false },
		_incomingAudioStreamRecorder{ &_incomingAudioStream },
		_incomingAudioHistograms{}
	{
		for (int i = 0; i < Clock::Instance().ChannelCount(); i++)
		{
			std::unique_ptr<Histogram> newHistogram{ new Histogram((int)Clock::Instance().TimeToSamples(MagicNumbers::RecentVolumeDuration).Value()) };
			_incomingAudioHistograms.push_back(std::move(newHistogram));
		}

		_inputDevice.AddOutgoingConnection(_frameOutputNode);
	}

	NowSoundInputInfo NowSoundInput::Info()
	{
		const std::unique_ptr<Histogram>& channel0 = _incomingAudioHistograms[0];
		const std::unique_ptr<Histogram>& channel1 = _incomingAudioHistograms[1];
		float channel0Volume = channel0->Average();
		float channel1Volume = channel1->Average();

		NowSoundInputInfo ret;
		ret.Channel0Volume = channel0Volume;
		ret.Channel1Volume = channel1Volume;
		ret.Pan = _pan;
		return ret;
	}

	void NowSoundInput::CreateRecordingTrack(TrackId id)
	{
		std::unique_ptr<NowSoundTrack> newTrack(new NowSoundTrack(id, _audioInputId, _incomingAudioStream));

		// new tracks are created as recording; lock the _recorders collection and add this new track
		{
			std::lock_guard<std::mutex> guard(_recorderMutex);
			_recorders.push_back(newTrack.get());
		}

		// move the new track over to the collection of tracks in NowSoundTrackAPI
		NowSoundTrack::AddTrack(id, std::move(newTrack));
	}

	const double Pi = std::atan(1) * 4;

	void NowSoundInput::HandleIncomingAudio()
	{
		AudioFrame frame = _frameOutputNode.GetFrame();

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
				latencyInSamples = (int)(Clock::Instance().SampleRateHz() * MagicNumbers::AudioFrameLengthSeconds.Value());
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
		float* dataInFloats = (float*)dataInBytes;
		for (int i = 0; i < duration.Value(); i++)
		{
			for (int j = 0; j < Clock::Instance().ChannelCount(); j++)
			{
				float value = dataInFloats[i * channelCount + j];
				// add the absolute value because for volume purposes we don't want negatives to cancel positives
				_incomingAudioHistograms[j]->Add(std::abs(value));
			}
		}

		float* bufferStartPointer = (float*)dataInBytes + bufferStart;

		// If we have a channelOpt setting, then copy the data into _buffer while panning it,
		// and use _buffer as the source for recording.
		if (_channelOpt.HasValue())
		{
			// Make sure our buffer is big enough.
			_buffer.resize(duration.Value() * Clock::Instance().ChannelCount());

			// Use cosine panner for volume preservation.
			double angularPosition = _pan * Pi / 2;
			double leftCoefficient = std::cos(angularPosition);
			double rightCoefficient = std::sin(angularPosition);
			for (int i = 0; i < duration.Value(); i++)
			{
				float inputValue = bufferStartPointer[(i * channelCount) + _channelOpt.Value()];

				_buffer[i * channelCount] = (float)(leftCoefficient * inputValue);
				_buffer[i * channelCount + 1] = (float)(rightCoefficient * inputValue);
			}

			bufferStartPointer = _buffer.data();
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
				bool stillRecording = recorder->Record(duration, bufferStartPointer);

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
