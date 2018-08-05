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

using namespace concurrency;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;
using namespace Windows::Media;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Render;
using namespace Windows::System;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;

namespace NowSound
{
	__declspec(dllexport) NowSoundGraphInfo NowSoundGraph_GetStaticGraphInfo()
	{
		return CreateNowSoundGraphInfo(
			1,
			2,
			3,
			(float)4,
			(float)5,
			(float)6);
	}

	NowSoundGraphState NowSoundGraph_State()
    {
        return NowSoundGraph::Instance()->State();
    }

    void NowSoundGraph_InitializeAsync()
    {
        NowSoundGraph::Instance()->InitializeAsync();
    }

    NowSoundDeviceInfo NowSoundGraph_GetDefaultRenderDeviceInfo()
    {
        return NowSoundGraph::Instance()->GetDefaultRenderDeviceInfo();
    }

    void NowSoundGraph_CreateAudioGraphAsync()
    {
        return NowSoundGraph::Instance()->CreateAudioGraphAsync();
    }

	NowSoundGraphInfo NowSoundGraph_Info()
	{
		return NowSoundGraph::Instance()->Info();
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
        : _audioGraph{ nullptr },
        _audioGraphState{ NowSoundGraphState::GraphUninitialized },
        _deviceOutputNode{ nullptr },
        _audioAllocator{
            ((int)Clock::SampleRateHz * MagicNumbers::AudioChannelCount * sizeof(float) * MagicNumbers::AudioBufferSizeInSeconds.Value()),
            MagicNumbers::InitialAudioBufferCount },
        _trackId{ TrackId::Undefined },
        _audioInputs{ },
        _audioInputsMutex{ },
        _changingState{ false }
    { }

    AudioGraph NowSoundGraph::GetAudioGraph() const { return _audioGraph; }

    AudioDeviceOutputNode NowSoundGraph::GetAudioDeviceOutputNode() const { return _deviceOutputNode; }

    BufferAllocator<float>* NowSoundGraph::GetAudioAllocator() { return &_audioAllocator; }

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
        _changingState = false;
        _audioGraphState = newState;
    }

    NowSoundGraphState NowSoundGraph::State()
    {
        std::lock_guard<std::mutex> guard(_stateMutex);
        return _audioGraphState;
    }

    void NowSoundGraph::InitializeAsync()
    {
        PrepareToChangeState(NowSoundGraphState::GraphUninitialized);
        // this does not need to be locked
        create_task([this]() -> IAsyncAction { co_await InitializeAsyncImpl(); });
    }

    IAsyncAction NowSoundGraph::InitializeAsyncImpl()
    {
		// MAKE THE CLOCK NOW.  It won't start running until the graph does.
		Clock::Initialize(
			MagicNumbers::InitialBeatsPerMinute,
			MagicNumbers::BeatsPerMeasure,
			MagicNumbers::AudioChannelCount);

        AudioGraphSettings settings(AudioRenderCategory::Media);
        settings.QuantumSizeSelectionMode(Windows::Media::Audio::QuantumSizeSelectionMode::LowestLatency);
        settings.DesiredRenderDeviceAudioProcessing(Windows::Media::AudioProcessing::Raw);
        // leaving PrimaryRenderDevice uninitialized will use default output device
        CreateAudioGraphResult result = co_await AudioGraph::CreateAsync(settings);

        if (result.Status() != AudioGraphCreationStatus::Success)
        {
            // Cannot create graph
            Check(false);
            return;
        }

        // NOTE that if this logic is inlined into the create_task lambda in InitializeAsync,
        // this assignment blows up saying that it is assigning to a value of 0xFFFFFFFFFFFF.
        // Probable compiler bug?  TODO: replicate the bug in test app.
        _audioGraph = result.Graph();

        ChangeState(NowSoundGraphState::GraphInitialized);
    }

    NowSoundDeviceInfo NowSoundGraph::GetDefaultRenderDeviceInfo()
    {
        return CreateNowSoundDeviceInfo(nullptr, nullptr);
    }

    void NowSoundGraph::CreateAudioGraphAsync(/*NowSound_DeviceInfo outputDevice*/) // TODO: output device selection?
    {
        // TODO: verify not on audio graph thread

        PrepareToChangeState(NowSoundGraphState::GraphInitialized);
        create_task([this]() -> IAsyncAction { co_await CreateAudioGraphAsyncImpl(); });
    }

    IAsyncAction NowSoundGraph::CreateAudioGraphAsyncImpl()
    {
		// Create a device output node
        CreateAudioDeviceOutputNodeResult deviceOutputNodeResult = co_await _audioGraph.CreateDeviceOutputNodeAsync();

        if (deviceOutputNodeResult.Status() != AudioDeviceNodeCreationStatus::Success)
        {
            // Cannot create device output node
            Check(false);
            return;
        }

        _deviceOutputNode = deviceOutputNodeResult.DeviceOutputNode();

		// Add the default audio input (don't need to lock yet since not *quite* running).
		std::unique_ptr<NowSoundInput> input(new NowSoundInput(this, AudioInputId::Input0, &_audioAllocator));
		_audioInputs.emplace_back(std::move(input));
		// now that we've moved it, take a new ref to it inside the collection
		std::unique_ptr<NowSoundInput>& inputRef = _audioInputs[0];

		_audioGraph.QuantumStarted([&](AudioGraph, IInspectable)
		{
			HandleIncomingAudio();
		});

		// now asynchronously complete initializing the audio input
		co_await inputRef->InitializeAsync();

        ChangeState(NowSoundGraphState::GraphCreated);
    }

    NowSoundGraphInfo NowSoundGraph::Info()
    {
        // TODO: verify not on audio graph thread

        Check(_audioGraphState >= NowSoundGraphState::GraphCreated);

		Time<AudioSample> now = Clock::Instance().Now();
		ContinuousDuration<Beat> durationBeats = Clock::Instance().TimeToBeats(now);
		int64_t completeBeats = (int64_t)durationBeats.Value();

		NowSoundGraphInfo graphInfo = CreateNowSoundGraphInfo(
			_audioGraph.LatencyInSamples(),
			_audioGraph.SamplesPerQuantum(),
			now.Value(),
			durationBeats.Value(),
			Clock::Instance().BeatsPerMinute(),
			(float)(completeBeats % Clock::Instance().BeatsPerMeasure())
		);
		return graphInfo;
    }

	NowSoundInputInfo NowSoundGraph::InputInfo(AudioInputId audioInputId)
	{
		Check(_audioGraphState >= NowSoundGraphState::GraphRunning);
		Check(audioInputId >= 0);
		Check(audioInputId < _audioInputs.size());

		std::unique_ptr<NowSoundInput>& input = _audioInputs[(int)audioInputId];
		return input->Info();
	}

    void NowSoundGraph::StartAudioGraphAsync()
    {
        // TODO: verify not on audio graph thread

        PrepareToChangeState(NowSoundGraphState::GraphCreated);

        // not actually async!  But let's not expose that, maybe this might be async later or we might add async stuff here.
        _audioGraph.Start();

        // As of now, we will start getting HandleIncomingAudio() callbacks.

        ChangeState(NowSoundGraphState::GraphRunning);
    }

    TrackId NowSoundGraph::CreateRecordingTrackAsync(AudioInputId audioInput)
    {
        // TODO: verify not on audio graph thread
        Check(_audioGraphState == NowSoundGraphState::GraphRunning);
		Check(audioInput >= 0);
		Check(audioInput < _audioInputs.size());

        // by construction this will be greater than TrackId::Undefined
        TrackId id = (TrackId)((int)_trackId + 1);
        _trackId = id;

		_audioInputs[(int)audioInput]->CreateRecordingTrack(id);

		return id;
    }

    IAsyncAction NowSoundGraph::PlayUserSelectedSoundFileAsyncImpl()
    {
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
		for (std::unique_ptr<NowSoundInput>& input : _audioInputs)
		{
			input->HandleIncomingAudio();
		}
    }
}
