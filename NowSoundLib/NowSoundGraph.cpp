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
	__declspec(dllexport) NowSoundGraphInfo NowSoundGraph_GetStaticGraphInfo()
	{
		return CreateNowSoundGraphInfo(
			1,
			2,
			3,
			4,
			5,
			6);
	}

	__declspec(dllexport) NowSoundTimeInfo NowSoundGraph_GetStaticTimeInfo()
	{
		return CreateNowSoundTimeInfo(
			1,
			2,
			(float)3,
			(float)4,
			(float)5);
	}

	NowSoundGraphState NowSoundGraph_State()
	{
		return NowSoundGraph::Instance()->State();
	}

	void NowSoundGraph_InitializeAsync()
	{
		NowSoundGraph::Instance()->InitializeAsync();
	}

	NowSoundGraphInfo NowSoundGraph_Info()
	{
		// externally, this can only be called once Initialize is complete; internally, NowSoundGraph::Info() is called *during* Initialize
		Check(NowSoundGraph::Instance()->State() >= NowSoundGraphState::GraphInitialized);
		return NowSoundGraph::Instance()->Info();
	}

	void NowSoundGraph_InputDeviceId(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity)
	{
		NowSoundGraph::Instance()->InputDeviceId(deviceIndex, wcharBuffer, bufferCapacity);
	}

	void NowSoundGraph_InputDeviceName(int deviceIndex, LPWSTR wcharBuffer, int bufferCapacity)
	{
		NowSoundGraph::Instance()->InputDeviceName(deviceIndex, wcharBuffer, bufferCapacity);
	}

	void NowSoundGraph_InitializeDeviceInputs(int deviceIndex)
	{
		NowSoundGraph::Instance()->InitializeDeviceInputs(deviceIndex);
	}

	void NowSoundGraph_InitializeFFT(
		int outputBinCount,
		float centralFrequency,
		int octaveDivisions,
		int centralBinIndex,
		int fftSize)
	{
		NowSoundGraph::Instance()->InitializeFFT(outputBinCount, centralFrequency, octaveDivisions, centralBinIndex, fftSize);
	}

	void NowSoundGraph_CreateAudioGraphAsync()
	{
		NowSoundGraph::Instance()->CreateAudioGraphAsync();
	}

	NowSoundTimeInfo NowSoundGraph_TimeInfo()
	{
		return NowSoundGraph::Instance()->TimeInfo();
	}

	NowSoundInputInfo NowSoundGraph_InputInfo(AudioInputId audioInputId)
	{
		return NowSoundGraph::Instance()->InputInfo(audioInputId);
	}

	void NowSoundGraph_StartAudioGraphAsync()
	{
		NowSoundGraph::Instance()->StartAudioGraphAsync();
	}

	void NowSoundGraph_PlayUserSelectedSoundFileAsync()
	{
		NowSoundGraph::Instance()->PlayUserSelectedSoundFileAsync();
	}

	void NowSoundGraph_DestroyAudioGraphAsync()
	{
		NowSoundGraph::Instance()->DestroyAudioGraphAsync();
	}

	TrackId NowSoundGraph_CreateRecordingTrackAsync(AudioInputId audioInputId)
	{
		return NowSoundGraph::Instance()->CreateRecordingTrackAsync(audioInputId);
	}

	TimeSpan timeSpanFromSeconds(int seconds)
	{
		// TimeSpan is in 100ns units
		return TimeSpan(seconds * Clock::TicksPerSecond);
	}

	std::unique_ptr<NowSoundGraph> NowSoundGraph::s_instance{ new NowSoundGraph() };

	NowSoundGraph* NowSoundGraph::Instance() { return s_instance.get(); }

	NowSoundGraph::NowSoundGraph()
		: // _audioGraph{ nullptr },
		_audioGraphState{ NowSoundGraphState::GraphUninitialized },
		_audioDeviceManager{},
		// _deviceOutputNode{ nullptr },
		_audioAllocator{ nullptr },
		_trackId{ TrackId::TrackIdUndefined },
		_nextAudioInputId{ AudioInputId::AudioInputUndefined },
		_inputDeviceIndicesToInitialize{},
		_audioInputs{ },
		_changingState{ false },
		_fftBinBounds{},
		_fftSize{ -1 }
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

	void NowSoundGraph::InitializeAsync()
	{
		PrepareToChangeState(NowSoundGraphState::GraphUninitialized);

		juce::String initialiseResult = _audioDeviceManager.initialise(
			/*numInputChannelsNeeded*/ 2,
			/*numOutputChannelsNeeded*/ 2,
			/*savedState*/ nullptr,
			/*selectDefaultDeviceOnFailure*/ true);

		// empty string means all good
		Check(initialiseResult == L"");

		NowSoundGraphInfo info = Info();

#if false

		// insist on stereo float samples.  TODO: generalize channel count
		Check(info.ChannelCount == 2);
		Check(info.BitsPerSample == 32);

		Clock::Initialize(
			info.SampleRateHz,
			info.ChannelCount,
			MagicConstants::InitialBeatsPerMinute,
			MagicConstants::BeatsPerMeasure);

		_audioAllocator = std::unique_ptr<BufferAllocator<float>>(new BufferAllocator<float>(
			(int)(Clock::Instance().BytesPerSecond() * MagicConstants::AudioBufferSizeInSeconds.Value()),
			MagicConstants::InitialAudioBufferCount));

		// save the local across the co_await statement
		std::vector<DeviceInformation>& inputDeviceInfoRef = _inputDeviceInfos;

		DeviceInformationCollection devices =
			co_await DeviceInformation::FindAllAsync(Windows::Media::Devices::MediaDevice::GetAudioCaptureSelector());

		// Translate all into _inputDeviceInfo entries
		for (Windows::Devices::Enumeration::DeviceInformation device : devices)
		{
			inputDeviceInfoRef.push_back(device);
		}

#endif

		ChangeState(NowSoundGraphState::GraphInitialized);
	}

	NowSoundGraphInfo NowSoundGraph::Info()
	{
		// TODO: verify not on audio graph thread
		juce::AudioIODevice* currentAudioDevice = _audioDeviceManager.getCurrentAudioDevice();

		NowSoundGraphInfo graphInfo = CreateNowSoundGraphInfo(
			currentAudioDevice->getCurrentSampleRate(),
			currentAudioDevice->getOutputChannelNames().size(),
			currentAudioDevice->getCurrentBitDepth(),
			0, // don't know how to get latency?
			currentAudioDevice->getCurrentBufferSizeSamples(),
			(int32_t)_inputDeviceInfos.size());
		return NowSoundGraphInfo();
	}

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

	const std::vector<RosettaFFT::FrequencyBinBounds>* NowSoundGraph::BinBounds() const { return &_fftBinBounds; }

	int NowSoundGraph::FftSize() const { return _fftSize; }

	void NowSoundGraph::CreateInputDeviceAsync(int deviceIndex)
	{
#if false
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
#endif
	}

#if false
	void NowSoundGraph::CreateInputDeviceFromNode(AudioDeviceInputNode inputNode, int channel)
	{
		AudioInputId nextAudioInputId(static_cast<AudioInputId>((int)(_audioInputs.size() + 1)));
		std::unique_ptr<NowSoundInput> input(new NowSoundInput(
			this,
			nextAudioInputId,
			inputNode,
			_audioAllocator.get(),
			channel));

		_audioInputs.emplace_back(std::move(input));
	}
#endif

	void NowSoundGraph::CreateAudioGraphAsync(/*NowSound_DeviceInfo outputDevice*/) // TODO: output device selection?
    {
        // TODO: verify not on audio graph thread

        PrepareToChangeState(NowSoundGraphState::GraphInitialized);

#if false
		// Create a device output node
        CreateAudioDeviceOutputNodeResult deviceOutputNodeResult = co_await _audioGraph.CreateDeviceOutputNodeAsync();

        if (deviceOutputNodeResult.Status() != AudioDeviceNodeCreationStatus::Success)
        {
            // Cannot create device output node
            Check(false);
            return;
        }

        _deviceOutputNode = deviceOutputNodeResult.DeviceOutputNode();

		_audioGraph.QuantumStarted([&](AudioGraph, IInspectable)
		{
			HandleIncomingAudio();
		});

		for (int i = 0; i < _inputDeviceIndicesToInitialize.size(); i++)
		{
			co_await CreateInputDeviceAsync(_inputDeviceIndicesToInitialize[i]);
		}
#endif

        ChangeState(NowSoundGraphState::GraphCreated);
    }

	NowSoundTimeInfo NowSoundGraph::TimeInfo()
	{
		// TODO: verify not on audio graph thread

		Check(_audioGraphState >= NowSoundGraphState::GraphCreated);

		Time<AudioSample> now = Clock::Instance().Now();
		ContinuousDuration<Beat> durationBeats = Clock::Instance().TimeToBeats(now);
		int64_t completeBeats = (int64_t)durationBeats.Value();

		NowSoundTimeInfo timeInfo = CreateNowSoundTimeInfo(
			(int32_t)_audioInputs.size(),
			now.Value(),
			durationBeats.Value(),
			Clock::Instance().BeatsPerMinute(),
			(float)(completeBeats % Clock::Instance().BeatsPerMeasure()));

		return timeInfo;
	}

	NowSoundInputInfo NowSoundGraph::InputInfo(AudioInputId audioInputId)
	{
		Check(_audioGraphState >= NowSoundGraphState::GraphCreated);

		Check(audioInputId > AudioInputId::AudioInputUndefined);
		// Input IDs are one-based
		Check((audioInputId - 1) < _audioInputs.size());

		std::unique_ptr<NowSoundInput>& input = _audioInputs[(int)audioInputId - 1];
		return input->Info();
	}

    void NowSoundGraph::StartAudioGraphAsync()
    {
        // TODO: verify not on audio graph thread

        PrepareToChangeState(NowSoundGraphState::GraphCreated);

        // not actually async!  But let's not expose that, maybe this might be async later or we might add async stuff here.
        // _audioGraph.Start();

        // As of now, we will start getting HandleIncomingAudio() callbacks.

        ChangeState(NowSoundGraphState::GraphRunning);
    }

	TrackId NowSoundGraph::CreateRecordingTrackAsync(AudioInputId audioInput)
    {
        // TODO: verify not on audio graph thread
        Check(_audioGraphState == NowSoundGraphState::GraphRunning);
		Check(audioInput >= 1);
		Check(audioInput < _audioInputs.size() + 1);

        // by construction this will be greater than TrackId::Undefined
        TrackId id = (TrackId)((int)_trackId + 1);
        _trackId = id;

		_audioInputs[(int)(audioInput - 1)]->CreateRecordingTrack(id);

		return id;
    }

    void NowSoundGraph::PlayUserSelectedSoundFileAsyncImpl()
    {
#if false
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

    void NowSoundGraph::DestroyAudioGraphAsync()
    {
    }

    void NowSoundGraph::HandleIncomingAudio()
    {
		// Clock::Instance().AdvanceFromAudioGraph(_audioGraph.SamplesPerQuantum());

		for (std::unique_ptr<NowSoundInput>& input : _audioInputs)
		{
			input->HandleIncomingAudio();
		}
    }
}
