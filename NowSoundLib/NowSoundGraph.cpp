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
#include "Option.h"

using namespace concurrency;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace winrt::Windows::Foundation;

namespace NowSound
{
	TimeSpan timeSpanFromSeconds(int seconds)
	{
		// TimeSpan is in 100ns units
		return TimeSpan(seconds * Clock::TicksPerSecond);
	}

	std::unique_ptr<NowSoundGraph> NowSoundGraph::s_instance{ nullptr };

	NowSoundGraph* NowSoundGraph::Instance() { return s_instance.get(); }

	void NowSoundGraph::InitializeInstance()
	{
		std::unique_ptr<NowSoundGraph> temp{ new NowSoundGraph() };
		s_instance = std::move(temp);
		s_instance.get()->Initialize();
	}

	NowSoundGraph::NowSoundGraph()
		: // _audioGraph{ nullptr },
		_audioGraphState{ NowSoundGraphState::GraphUninitialized },
		_audioDeviceManager{},
		// _deviceOutputNode{ nullptr },
		_audioAllocator{ nullptr },
		_trackId{ TrackId::TrackIdUndefined },
		_nextAudioInputId{ AudioInputId::AudioInputUndefined },
		// JUCETODO: _inputDeviceIndicesToInitialize{},
		_audioInputs{ },
		_changingState{ false },
		_fftBinBounds{},
		_fftSize{ -1 },
		_stateMutex{}
	{ }

	// AudioGraph NowSoundGraph::GetAudioGraph() const { return _audioGraph; }

	// AudioDeviceOutputNode NowSoundGraph::AudioDeviceOutputNode() const { return _deviceOutputNode; }

	BufferAllocator<float>* NowSoundGraph::AudioAllocator() const { return _audioAllocator.get(); }

	void NowSoundGraph::PrepareToChangeState(NowSoundGraphState expectedState)
	{
		std::lock_guard<std::mutex> guard(_stateMutex);

		Check(_audioGraphState == expectedState);
		Check(!_changingState);
		_changingState = true;
	}

	void NowSoundGraph::ChangeState(NowSoundGraphState newState)
	{
		std::lock_guard<std::mutex> guard(_stateMutex);

		Check(_changingState);
		Check(newState != _audioGraphState);
		_changingState = false;
		_audioGraphState = newState;
	}

	NowSoundGraphState NowSoundGraph::State() const
	{
		// this is a machine word, atomically written; no need to lock
		return _audioGraphState;
	}

	void NowSoundGraph::Initialize()
	{
		PrepareToChangeState(NowSoundGraphState::GraphUninitialized);

		// Valid values for this on Surface Book are L"DirectSound" and L"Windows Audio".
		// Both seem to permit 96 sample buffering (!) but DirectSound seems to come up with
		// 16 bit audio sometimes... let's see if Windows Audio (AKA (AFAICT) WASAPI) causes
		// similar heartache.
		juce::String desiredDeviceType = L"Windows Audio";

#ifdef INITIALIZATION_HACKERY
		{
			// This whole code block was an experiment in trying to force the device type to be
			// DirectSound before initialization, to avoid the 1500msec Thread.Sleep when setting
			// the device type after initialization.
			// If Windows Audio is the desired AND default device type anyway, then all of this
			// can get punted out the window and we can just call initialise() with no rigmarole.
			_audioDeviceManager.createDeviceTypesIfNeeded();
			const OwnedArray<AudioIODeviceType>& types = _audioDeviceManager.getAvailableDeviceTypes();
			for (int i = 0; i < types.size(); i++)
			{
				String typeName(types[i]->getTypeName());  // This will be things like "DirectSound", "CoreAudio", etc.
				if (typeName == desiredDeviceType)
				{
					types[i]->scanForDevices();
					StringArray deviceNames(types[i]->getDeviceNames());  // This will now return a list of available devices of this type
				}
			}
		}
		// NOW we can call this...?????  But it seems kind of stupid since it goes through the whole initialise() path.
		_audioDeviceManager.setCurrentAudioDeviceType(desiredDeviceType, /*treatAsChosenDevice*/ true);
#endif

		AudioDeviceManager::AudioDeviceSetup setup{};
		setup.bufferSize = 96;
		setup.sampleRate = 48000;
		setup.useDefaultInputChannels = true;
		setup.useDefaultOutputChannels = true;

		juce::String initialiseResult = _audioDeviceManager.initialise(
			/*numInputChannelsNeeded*/ 2,
			/*numOutputChannelsNeeded*/ 2,
			/*savedState*/ nullptr,
			/*selectDefaultDeviceOnFailure*/ true,
			/*preferredDefaultDeviceName*/ String(),
			/*preferredSetupOptions*/ &setup);

		// empty string means all good; anything else means error
		if (initialiseResult != L"")
		{
			// TODO: save the result for diagnostic reporting
			ChangeState(NowSoundGraphState::GraphInError);
			return;
		}

		int minBufferSize = _audioDeviceManager.getCurrentAudioDevice()->getAvailableBufferSizes()[0];
		_audioDeviceManager.getAudioDeviceSetup(setup);
		setup.bufferSize = minBufferSize;

		// setting treatAsChosenDevice to true can leak an XmlElement... not sure what's up with that
		_audioDeviceManager.setAudioDeviceSetup(setup, /*treatAsChosenDevice*/ false);

		// now check that it was effective
		_audioDeviceManager.getAudioDeviceSetup(setup);
		Check(minBufferSize == setup.bufferSize);

		// we expect WASAPI, and we don't want this to wiggle around on us without us realizing we're
		// suddenly using something else; otherwise all our test results will be incomplete
		Check(_audioDeviceManager.getCurrentAudioDeviceType() == L"Windows Audio");

		NowSoundGraphInfo info = Info();

		// insist on stereo float samples.  TODO: generalize channel count
		// For right now let's just make absolutely sure these values are all precisely as we intend every time.
		Check(info.ChannelCount == 2);
		Check(info.BitsPerSample == 32);
		Check(info.SamplesPerQuantum == minBufferSize);

		Clock::Initialize(
			info.SampleRateHz,
			info.ChannelCount,
			MagicConstants::InitialBeatsPerMinute,
			MagicConstants::BeatsPerMeasure);

		_audioAllocator = std::unique_ptr<BufferAllocator<float>>(new BufferAllocator<float>(
			(int)(Clock::Instance().BytesPerSecond() * MagicConstants::AudioBufferSizeInSeconds.Value()),
			MagicConstants::InitialAudioBufferCount));

		// set up audio processor player and graph
		juce::AudioProcessorGraph::AudioGraphIOProcessor* inputNode =
			new juce::AudioProcessorGraph::AudioGraphIOProcessor(juce::AudioProcessorGraph::AudioGraphIOProcessor::IODeviceType::audioInputNode);
		juce::AudioProcessorGraph::AudioGraphIOProcessor* outputNode =
			new juce::AudioProcessorGraph::AudioGraphIOProcessor(juce::AudioProcessorGraph::AudioGraphIOProcessor::IODeviceType::audioOutputNode);

		juce::AudioProcessorGraph::Node::Ptr inputNodePtr = _audioProcessorGraph.addNode(inputNode);
		juce::AudioProcessorGraph::Node::Ptr outputNodePtr = _audioProcessorGraph.addNode(outputNode);
		for (int i = 0; i < info.ChannelCount; i++)
		{
			CreateInputDeviceForChannel(i);
			_audioProcessorGraph.addConnection({ { inputNodePtr->nodeID, i }, { outputNodePtr->nodeID, i } });
		}

		_audioProcessorPlayer.setProcessor(&_audioProcessorGraph);

		ChangeState(NowSoundGraphState::GraphRunning);
	}

	NowSoundGraphInfo NowSoundGraph::Info()
	{
		// TODO: verify not on audio graph thread
		juce::AudioIODevice* currentAudioDevice = _audioDeviceManager.getCurrentAudioDevice();

		auto availableBufferSizes = currentAudioDevice->getAvailableBufferSizes();

		NowSoundGraphInfo graphInfo = CreateNowSoundGraphInfo(
			currentAudioDevice->getCurrentSampleRate(),
			currentAudioDevice->getOutputChannelNames().size(),
			currentAudioDevice->getCurrentBitDepth(),
			currentAudioDevice->getOutputLatencyInSamples(),
			currentAudioDevice->getCurrentBufferSizeSamples());
		return graphInfo;
	}

#ifdef INPUT_DEVICE_SELECTION
	void NowSoundGraph::InputDeviceId(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity)
	{
		// wcsncpy_s(wcharBuffer, bufferCapacity, _inputDeviceInfos[deviceIndex].Id().c_str(), _TRUNCATE);
	}

	void NowSoundGraph::InputDeviceName(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity)
	{
		// wcsncpy_s(wcharBuffer, bufferCapacity, _inputDeviceInfos[deviceIndex].Name().c_str(), _TRUNCATE);
	}

	void NowSoundGraph::InitializeDeviceInputs(int deviceIndex)
	{
		Check(State() == NowSoundGraphState::GraphInitialized);

		_inputDeviceIndicesToInitialize.push_back(deviceIndex);
	}
#endif

	void NowSoundGraph::InitializeFFT(
		int outputBinCount,
		float centralFrequency,
		int octaveDivisions,
		int centralBinIndex,
		int fftSize)
	{
		_fftBinBounds.resize(outputBinCount);
		_fftSize = fftSize;

		// Initialize the bounds of the bins into which we collate FFT data.
		RosettaFFT::MakeBinBounds(
			_fftBinBounds,
			centralFrequency,
			octaveDivisions,
			outputBinCount,
			centralBinIndex,
			Clock::Instance().SampleRateHz(),
			fftSize);
	}

	const std::vector<std::unique_ptr<NowSoundInput>>& NowSoundGraph::Inputs() const { return _audioInputs; }

	const std::vector<RosettaFFT::FrequencyBinBounds>* NowSoundGraph::BinBounds() const { return &_fftBinBounds; }

	int NowSoundGraph::FftSize() const { return _fftSize; }

#ifdef JUCETODO
	void NowSoundGraph::CreateInputDeviceAsync(int deviceIndex)
	{
		// Create a device input node
		CreateAudioDeviceInputNodeResult deviceInputNodeResult = co_await _audioGraph.CreateDeviceInputNodeAsync(
			Windows::Media::Capture::MediaCategory::Media,
			_audioGraph.EncodingProperties(),
			_inputDeviceInfos[deviceIndex]);

		if (deviceInputNodeResult.Status() != AudioDeviceNodeCreationStatus::Success)
		{
			// Cannot create device output node
			Check(false);
			return;
		}

		AudioDeviceInputNode inputNode = deviceInputNodeResult.DeviceInputNode();

		// create one AudioInput per input channel of the device
		for (uint32_t i = 0; i < inputNode.EncodingProperties().ChannelCount(); i++)
		{
			CreateInputDeviceFromNode(inputNode, (int)i);
		}
	}
#endif

	void NowSoundGraph::CreateInputDeviceForChannel(int channel)
	{
		AudioInputId nextAudioInputId(static_cast<AudioInputId>((int)(_audioInputs.size() + 1)));
		std::unique_ptr<NowSoundInput> input(new NowSoundInput(
			this,
			nextAudioInputId,
			_audioAllocator.get(),
			channel));

		_audioInputs.emplace_back(std::move(input));
	}

	NowSoundTimeInfo NowSoundGraph::TimeInfo()
	{
		// TODO: verify not on audio graph thread

		Check(_audioGraphState > NowSoundGraphState::GraphInError);

		Time<AudioSample> now = Clock::Instance().Now();
		ContinuousDuration<Beat> durationBeats = Clock::Instance().TimeToBeats(now);
		int64_t completeBeats = (int64_t)durationBeats.Value();

		NowSoundTimeInfo timeInfo = CreateNowSoundTimeInfo(
			// JUCETODO: (int32_t)_audioInputs.size(),
			now.Value(),
			durationBeats.Value(),
			Clock::Instance().BeatsPerMinute(),
			(float)(completeBeats % Clock::Instance().BeatsPerMeasure()));

		return timeInfo;
	}

	NowSoundInputInfo NowSoundGraph::InputInfo(AudioInputId audioInputId)
	{
		Check(_audioGraphState > NowSoundGraphState::GraphInError);

		Check(audioInputId > AudioInputId::AudioInputUndefined);
		// Input IDs are one-based
		Check((audioInputId - 1) < _audioInputs.size());

		std::unique_ptr<NowSoundInput>& input = _audioInputs[(int)audioInputId - 1];
		return input->Info();
	}

	TrackId NowSoundGraph::CreateRecordingTrackAsync(AudioInputId audioInput)
    {
        // TODO: verify not on audio graph thread
        Check(_audioGraphState == NowSoundGraphState::GraphRunning);
		Check(audioInput >= 1);
		// Check(audioInput < _audioInputs.size() + 1);

        // by construction this will be greater than TrackId::Undefined
        TrackId id = (TrackId)((int)_trackId + 1);
        _trackId = id;

		_audioInputs[(int)(audioInput - 1)]->CreateRecordingTrack(id);

		return id;
    }

    void NowSoundGraph::PlayUserSelectedSoundFileAsyncImpl()
    {
#if JUCETODO
        // This must be called on the UI thread.
        FileOpenPicker picker;
        picker.SuggestedStartLocation(PickerLocationId::MusicLibrary);
        picker.FileTypeFilter().Append(L".wav");
        StorageFile file = co_await picker.PickSingleFileAsync();

        if (!file)
        {
            Check(false);
            return;
        }

        CreateAudioFileInputNodeResult fileInputResult = co_await _audioGraph.CreateFileInputNodeAsync(file);
        if (AudioFileNodeCreationStatus::Success != fileInputResult.Status())
        {
            // Cannot read input file
            Check(false);
            return;
        }

        AudioFileInputNode fileInput = fileInputResult.FileInputNode();

        if (fileInput.Duration() <= timeSpanFromSeconds(3))
        {
            // Imported file is too short
            Check(false);
            return;
        }

        fileInput.AddOutgoingConnection(_deviceOutputNode);
        fileInput.Start();
#endif
    }

    void NowSoundGraph::PlayUserSelectedSoundFileAsync()
    {
        PlayUserSelectedSoundFileAsyncImpl();
    }

    void NowSoundGraph::ShutdownInstance()
    {
		// drop the singleton
		s_instance = nullptr;
    }
}
