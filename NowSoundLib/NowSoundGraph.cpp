// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "Clock.h"
#include "NowSoundLib.h"

using namespace NowSound;
using namespace concurrency;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Ren;
using namespace Windows::System;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;

TimeSpan timeSpanFromSeconds(int seconds)
{
    // TimeSpan is in 100ns units
    return TimeSpan(seconds * 10000000);
}

static NowSoundGraph_State s_audioGraphState{ NowSoundGraph_State::Uninitialized };

static AudioGraph s_audioGraph{ nullptr };

// TODO: really really need a real graph node store
AudioDeviceOutputNode s_deviceOutputNode{ nullptr };

// First, an allocator for 128-second 48Khz stereo float sample buffers.
BufferAllocator<float> s_audioAllocator(((int)Clock::SampleRateHz * 2 * sizeof(float)), 128);

AudioGraph NowSoundGraph::GetAudioGraph() { return s_audioGraph; }

AudioDeviceOutputNode NowSoundGraph::GetAudioDeviceOutputNode() { return s_deviceOutputNode; }

BufferAllocator<float>* NowSoundGraph::GetAudioAllocator() { return &s_audioAllocator; }

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
    });
}

NowSound_DeviceInfo NowSoundGraph::NowSoundGraph_GetDefaultRenderDeviceInfo()
{
    return NowSound_DeviceInfo(nullptr, nullptr);
}

void NowSoundGraph::NowSoundGraph_CreateAudioGraphAsync(NowSound_DeviceInfo outputDevice)
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
