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
#include "Option.h"

using namespace concurrency;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Devices;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Pickers;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Composition;

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
		// _deviceOutputNode{ nullptr },
		_audioAllocator{ nullptr },
		_trackId{ TrackId::TrackIdUndefined },
		_nextAudioInputId{ AudioInputId::AudioInputUndefined },
		_inputDeviceIndicesToInitialize{},
		_audioInputs{ },
		_changingState{ false },
		_fftBinBounds{},
		_fftSize{ -1 },
		_sampleReadyEvent{ CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS) },
		_bufferFrames{ 0 },
		_sampleReadyKey{},
		_sampleMutex{},
		_audioClientFormat{},
		_defaultPeriodInFrames{},
		_fundamentalPeriodInFrames{},
		_maxPeriodInFrames{},
		_minPeriodInFrames{},
		_audioClient{},
		_audioRenderClient{},
		_sampleReadyAsyncResult{},
		_deviceProps{},
		_handler{ this }
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

	HRESULT NowSoundGraph::NowSoundGraphActivationHandler::ActivateCompleted(IActivateAudioInterfaceAsyncOperation *operation)
	{
		_graph->ContinueActivation(operation);
		return S_OK;
	}

	void NowSoundGraph::InitializeAsync()
	{
		PrepareToChangeState(NowSoundGraphState::GraphUninitialized);

		winrt::hstring defaultRenderDeviceId = MediaDevice::GetDefaultAudioRenderId(AudioDeviceRole::Default);
		IActivateAudioInterfaceAsyncOperation *asyncOp;

		check_hresult(ActivateAudioInterfaceAsync(
			defaultRenderDeviceId.c_str(),
			__uuidof(IAudioClient3),
			nullptr,
			&_handler,
			&asyncOp));

		// manually expanded code for "SAFE_RELEASE(asyncOp);" in original code
		if (asyncOp != nullptr)
		{
			asyncOp->Release();
			asyncOp = nullptr;
		}
	}

	// the only sample rates that are particularly relevant for our purposes
	const int SampleRatesToProbe[] = { 44100, 48000, 88200, 96000 };

	const int BitsPerSampleToProbe[] = { 16, 24, 32 };

	uint32_t ConvertHnsToFrames(REFERENCE_TIME hnsPeriod, uint32_t samplesPerSec)
	{
		return (uint32_t)( // frames =
			1.0 * hnsPeriod * // hns *
			samplesPerSec / // (frames / s) /
			1000 / // (ms / s) /
			10000 // (hns / s) /
			+ 0.5); // rounding
	}

	REFERENCE_TIME ConvertFramesToHns(uint32_t frames, uint32_t samplesPerSec)
	{
		return (REFERENCE_TIME)(
			10000.0 * // (hns / ms) *
			1000 * // (ms / s) *
			frames / // frames /
			samplesPerSec  // (frames / s)
			+ 0.5); // rounding
	}

	void NowSoundGraph::ContinueActivation(IActivateAudioInterfaceAsyncOperation *operation)
	{
		HRESULT hrActivateResult = S_OK;
		// Is this best practice?  Compare to "reinterpret_cast to IUnknown**" guidance on 
		// https://docs.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/consume-com
		winrt::com_ptr<::IUnknown> punkAudioInterface;
		check_hresult(operation->GetActivateResult(&hrActivateResult, punkAudioInterface.put()));
		_audioClient = punkAudioInterface.as<IAudioClient3>();

		// Calculate whatever GraphInfo fields are easier to cache on initialization (especially sample rate).
		// WASAPI provides the IsFormatSupported query-like API to determine the full set of hardware capabilities,
		// but this is overkill for our purposes so we just use it simply once upfront; any data we get from it
		// we cache for later calls to Info().
		std::wstringstream wstr{};
		int32_t desiredRate = 48000; // we will stick with this for now
		int32_t desiredBitsPerSample = 32; // float is our friend
		bool desiredFormatSupported = false;

		for (int candidateSampleRate : SampleRatesToProbe)
		{
			for (int bitsPerSample : BitsPerSampleToProbe)
			{
				WAVEFORMATEX format{ 0 };
				format.wFormatTag = WAVE_FORMAT_PCM;
				format.nChannels = 2; // TODO: extend beyond stereo
				format.nSamplesPerSec = candidateSampleRate;
				format.nAvgBytesPerSec = format.nChannels * candidateSampleRate * (bitsPerSample / 8);
				format.wBitsPerSample = (WORD)bitsPerSample; // won't settle for less, sorry
				format.nBlockAlign = format.wBitsPerSample / 8;
				format.cbSize = 0;

				bool isSupported = _audioClient->IsFormatSupported(
					AUDCLNT_SHAREMODE_EXCLUSIVE, // we want full control
					&format, // see if this format is valid
					nullptr); // exclusive mode doesn't support "closest available mode"

				wstr << "Sample rate " << candidateSampleRate << ", bitsPerSample " << bitsPerSample << ", exclusive: " << (isSupported ? "YES" : "NO") << std::endl;

				if (isSupported && (desiredRate == candidateSampleRate) && (desiredBitsPerSample == bitsPerSample))
				{
					desiredFormatSupported = true;
					_audioClientFormat = format;
				}
			}
		}

		std::wstring ws = wstr.str();

		Check(desiredFormatSupported);

		// Is offload capable?
		BOOL isOffloadCapable;
		check_hresult(_audioClient->IsOffloadCapable(AUDIO_STREAM_CATEGORY::AudioCategory_Media, &isOffloadCapable));
		Check(isOffloadCapable); // all good audio interfaces are, you know

		/* Experiment: doesn't seem to work -- this call succeeds, but later causes 0x8889025 (AUDCLNT_E_NONOFFLOAD_MODE_ONLY) error from Initialize
		AudioClientProperties audioClientProperties{};
		audioClientProperties.cbSize = sizeof(AudioClientProperties);
		audioClientProperties.bIsOffload = TRUE;
		audioClientProperties.eCategory = AudioCategory_Media;
		check_hresult(_audioClient->SetClientProperties(&audioClientProperties));
		*/

		// get the periodicity of the device
		REFERENCE_TIME hnsPeriod;
		check_hresult(_audioClient->GetDevicePeriod(
			NULL, // don't care about the engine period
			&hnsPeriod)); // only the device period

		// need to know how many frames that is
		uint32_t expectedFramesInBuffer = ConvertHnsToFrames(hnsPeriod, _audioClientFormat.nSamplesPerSec);
		// had better be block-aligned
		Check((expectedFramesInBuffer % _audioClientFormat.nBlockAlign) == 0);
		wstr << "The default period for this device is " << hnsPeriod << " hundred-nanoseconds, or " << expectedFramesInBuffer << " frames." << std::endl;

		// HACK... we cheated and we know the number of frames should be 128, not 104.
		REFERENCE_TIME hnsActualPeriod = ConvertFramesToHns(128, _audioClientFormat.nSamplesPerSec);

		/* Evidently can't call this before initialization...?
		REFERENCE_TIME minBufferDuration, maxBufferDuration;
		check_hresult(_audioClient->GetBufferSizeLimits(
			&_audioClientFormat,
			true, // yes, event driven
			&minBufferDuration,
			&maxBufferDuration));

		// what about the minimum buffer size?
		uint32_t expectedFramesInMinimumBuffer = ConvertHnsToFrames(minBufferDuration, _audioClientFormat.nSamplesPerSec);

		wstr << "The minimum buffer duration is " << minBufferDuration << " hundred-nanoseconds, or " << expectedFramesInMinimumBuffer << " frames." << std::endl;
		*/

		ws = wstr.str();

		// call IAudioClient::Initialize the first time
		// this may very well fail if the device period is unaligned
		// TODO: HELP: why does this not succeed, when _audioClientFormat was confirmed as being supported in the loop above?
		HRESULT hr = _audioClient->Initialize(
			AUDCLNT_SHAREMODE_EXCLUSIVE,
			AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			hnsActualPeriod, // hnsPeriod,
			hnsActualPeriod, // hnsPeriod,
			&_audioClientFormat,
			nullptr);

		uint32_t actualFramesInBuffer;

		if (AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED == hr)
		{
			// this is sort of half-initialized...
			check_hresult(_audioClient->GetBufferSize(&actualFramesInBuffer));
			Check(actualFramesInBuffer != expectedFramesInBuffer);
			wstr << "Buffer size not aligned, can't yet support the alignment dance" << std::endl;
			// get the buffer size, which will be aligned

			// throw away this IAudioClient
			// TODO: can't we just call Initialize on it again???
			_audioClient = nullptr;

			// calculate the new aligned periodicity
			hnsPeriod = ConvertFramesToHns(actualFramesInBuffer, _audioClientFormat.nSamplesPerSec);

			Check(false);
			// eventually, implement the below... but asynchronously... yeesh

			/*
			// activate a new IAudioClient
			hr = pMMDevice->Activate(
				__uuidof(IAudioClient),
				CLSCTX_ALL, NULL,
				(void**)&pAudioClient
			);
			if (FAILED(hr)) {
				printf("IMMDevice::Activate(IAudioClient) failed: hr = 0x%08x\n", hr);
				return hr;
			}

			// try initialize again
			printf("Trying again with periodicity of %I64u hundred-nanoseconds, or %u frames.\n", hnsPeriod, nFramesInBuffer);
			hr = pAudioClient->Initialize(
				AUDCLNT_SHAREMODE_EXCLUSIVE,
				AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
				hnsPeriod, hnsPeriod, pWfx, NULL
			);

			if (FAILED(hr))
			{
				printf("IAudioClient::Initialize failed, even with an aligned buffer: hr = 0x%08x\n", hr);
				pAudioClient->Release();
				return hr;
			}
			*/
		}
		else
		{
			check_hresult(hr);
		}

		NowSoundGraphInfo info = Info();

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

		/* TODO: WASAPI device info collection
		// save the local across the co_await statement
		std::vector<DeviceInformation>& inputDeviceInfoRef = _inputDeviceInfos;

		DeviceInformationCollection devices =
			co_await DeviceInformation::FindAllAsync(Windows::Media::Devices::MediaDevice::GetAudioCaptureSelector());

		// Translate all into _inputDeviceInfo entries
		for (Windows::Devices::Enumeration::DeviceInformation device : devices)
		{
			inputDeviceInfoRef.push_back(device);
		}
		*/

		ChangeState(NowSoundGraphState::GraphInitialized);
	}

	NowSoundGraphInfo NowSoundGraph::Info()
	{
		// TODO: verify not on audio graph thread
		NowSoundGraphInfo graphInfo = CreateNowSoundGraphInfo(
			_audioClientFormat.nSamplesPerSec,
			_audioClientFormat.nChannels,
			_audioClientFormat.wBitsPerSample,
			0, // TODO: _audioGraph.LatencyInSamples(),
			0, // TODO: _audioGraph.SamplesPerQuantum(),
			0); // TODO: (int32_t)_inputDeviceInfos.size());

		NowSoundGraphInfo info{};
		return info;
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
