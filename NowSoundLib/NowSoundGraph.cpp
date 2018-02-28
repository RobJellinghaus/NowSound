// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "Clock.h"
#include "NowSoundLib.h"
#include "NowSoundTrack.h"

using namespace NowSound;
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
using namespace Windows::System;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;

TimeSpan timeSpanFromSeconds(int seconds)
{
    // TimeSpan is in 100ns units
    return TimeSpan(seconds * Clock::TicksPerSecond);
}

static NowSoundGraph_State s_audioGraphState{ NowSoundGraph_State::Uninitialized };

static AudioGraph s_audioGraph{ nullptr };

// The default output device. TODO: support multiple output devices.
AudioDeviceOutputNode s_deviceOutputNode{ nullptr };

// First, an allocator for 128-second 48Khz stereo float sample buffers.
BufferAllocator<float> s_audioAllocator(((int)Clock::SampleRateHz * 2 * sizeof(float)), 128);

// Audio frame, reused for copying audio.
AudioFrame s_audioFrame{ nullptr };

// The default input device. TODO: input device selection.
AudioDeviceInputNode s_defaultInputDevice{ nullptr };

// This FrameOutputNode delivers the data from the (default) input device. TODO: support multiple input devices.
AudioFrameOutputNode s_inputDeviceFrameOutputNode{ nullptr };

// The next TrackId to be allocated.
TrackId s_trackId{ 0 };

AudioGraph NowSoundGraph::GetAudioGraph() { return s_audioGraph; }

AudioDeviceOutputNode NowSoundGraph::GetAudioDeviceOutputNode() { return s_deviceOutputNode; }

BufferAllocator<float>* NowSoundGraph::GetAudioAllocator() { return &s_audioAllocator; }

AudioFrame NowSoundGraph::GetAudioFrame() { return s_audioFrame; }

NowSoundGraph_State NowSoundGraph::NowSoundGraph_GetGraphState()
{
    return s_audioGraphState;
}

void NowSoundGraph::NowSoundGraph_InitializeAsync()
{
    Check(s_audioGraphState == NowSoundGraph_State::Uninitialized);
    create_task([]() -> IAsyncAction
    {
        AudioGraphSettings settings(AudioRenderCategory::Media);
        settings.QuantumSizeSelectionMode(Windows::Media::Audio::QuantumSizeSelectionMode::LowestLatency);
        settings.DesiredRenderDeviceAudioProcessing(Windows::Media::AudioProcessing::Raw);
        // leaving PrimaryRenderDevice uninitialized will use default output device
        CreateAudioGraphResult result = co_await AudioGraph::CreateAsync(settings);

        if (result.Status() != AudioGraphCreationStatus::Success)
        {
            // Cannot create graph
            CoreApplication::Exit();
            return;
        }

        s_audioGraph = result.Graph();

        s_audioGraphState = NowSoundGraph_State::Initialized;

        s_audioFrame = AudioFrame(Clock::SampleRateHz / 4 * sizeof(float) * 2);
    });
}

NowSound_DeviceInfo NowSoundGraph::NowSoundGraph_GetDefaultRenderDeviceInfo()
{
    return NowSound_DeviceInfo(nullptr, nullptr);
}

void NowSoundGraph::NowSoundGraph_CreateAudioGraphAsync(/*NowSound_DeviceInfo outputDevice*/) // TODO: output device selection?
{
    Check(s_audioGraphState == NowSoundGraph_State::Initialized);

    create_task([]() -> IAsyncAction
    {
        // Create a device output node
        CreateAudioDeviceOutputNodeResult deviceOutputNodeResult = co_await s_audioGraph.CreateDeviceOutputNodeAsync();

        if (deviceOutputNodeResult.Status() != AudioDeviceNodeCreationStatus::Success)
        {
            // Cannot create device output node
            CoreApplication::Exit();
            return;
        }

        s_deviceOutputNode = deviceOutputNodeResult.DeviceOutputNode();

        // Create a device input node
        CreateAudioDeviceInputNodeResult deviceInputNodeResult = co_await s_audioGraph.CreateDeviceInputNodeAsync();

        if (deviceInputNodeResult.Status() != AudioDeviceNodeCreationStatus::Success)
        {
            // Cannot create device input node
            CoreApplication::Exit();
            return;
        }

        s_defaultInputDevice = deviceInputNodeResult.DeviceInputNode();
        s_inputDeviceFrameOutputNode = s_audioGraph.CreateFrameOutputNode();
        s_defaultInputDevice.AddOutgoingConnection(s_inputDeviceFrameOutputNode);

        // TODO: add Graph_QuantumStarted handler
        ...

        s_audioGraphState = NowSoundGraph_State::Created;
    });
}

NowSound_GraphInfo NowSoundGraph::NowSoundGraph_GetGraphInfo()
{
    Check(s_audioGraphState >= NowSoundGraph_State::Created);
    return NowSound_GraphInfo(s_audioGraph.LatencyInSamples(), s_audioGraph.SamplesPerQuantum());
}

void NowSoundGraph::NowSoundGraph_StartAudioGraphAsync()
{
    Check(s_audioGraphState == NowSoundGraph_State::Created);

    s_audioGraph.Start();

    s_audioGraphState = NowSoundGraph_State::Running;
}

TrackId NowSoundGraph::NowSoundGraph_CreateRecordingTrackAsync()
{
    // TODO: confirm we are on UI thread.
    // Since we are mutating the audio graph, we must not be on the audio graph thread.
    Check(s_audioGraphState == NowSoundGraph_State::Running);

    TrackId id = s_trackId++;

    std::unique_ptr<NowSoundTrack> newTrack(new NowSoundTrack(id, AudioInputId::Input0));
    NowSoundTrackAPI::AddTrack(id, std::move(newTrack));

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
        CoreApplication::Exit();
        return;
    }

    CreateAudioFileInputNodeResult fileInputResult = co_await s_audioGraph.CreateFileInputNodeAsync(file);
    if (AudioFileNodeCreationStatus::Success != fileInputResult.Status())
    {
        // Cannot read input file
        CoreApplication::Exit();
        return;
    }

    AudioFileInputNode fileInput = fileInputResult.FileInputNode();

    if (fileInput.Duration() <= timeSpanFromSeconds(3))
    {
        // Imported file is too short
        CoreApplication::Exit();
        return;
    }

    fileInput.AddOutgoingConnection(s_deviceOutputNode);
    fileInput.Start();
}

void NowSoundGraph::NowSoundGraph_PlayUserSelectedSoundFileAsync()
{
    PlayUserSelectedSoundFileAsyncImpl();
}

void NowSoundGraph::NowSoundGraph_DestroyAudioGraphAsync()
{
}

void NowSoundGraph::HandleIncomingAudio()
{
    AudioFrame frame = frameOutputNode.GetFrame();

    uint8_t* dataInBytes{};
    uint32_t capacityInBytes{};

    // OMG KENNY KERR WINS AGAIN:
    // https://gist.github.com/kennykerr/f1d941c2d26227abbf762481bcbd84d3
    Windows::Media::AudioBuffer buffer(NowSoundGraph::GetAudioFrame().LockBuffer(Windows::Media::AudioBufferAccessMode::Write));
    IMemoryBufferReference reference(buffer.CreateReference());
    winrt::impl::com_ref<IMemoryBufferByteAccess> interop = reference.as<IMemoryBufferByteAccess>();
    check_hresult(interop->GetBuffer(&dataInBytes, &capacityInBytes));

}

void NowSoundTrackAPI::AddTrack(TrackId id, std::unique_ptr<NowSoundTrack>&& track)
{
    _tracks.emplace(id, std::move(track));
}