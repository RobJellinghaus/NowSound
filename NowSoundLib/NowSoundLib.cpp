// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

using namespace std::chrono;
using namespace winrt;

using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Render;
using namespace Windows::System;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;

TimeSpan timeSpanFromSeconds(int seconds)
{
	// TimeSpan is in 100ns units
	return TimeSpan(seconds * 10000000);
}

// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "Contract.h"
#include "NowSoundLib.h"

using namespace NowSound;
using namespace concurrency;
using namespace std;

static AudioGraphState s_audioGraphState{ AudioGraphState::Uninitialized };

static AudioGraph s_audioGraph{ nullptr };

AudioGraphState HolofunkAudioGraph::HolofunkAudioGraph_GetGraphState()
{
	return s_audioGraphState;
}

void HolofunkAudioGraph::HolofunkAudioGraph_InitializeAsync()
{
	Contract::Requires(s_audioGraphState == AudioGraphState::Uninitialized);
	create_task([]() -> IAsyncAction
	{
		AudioGraphSettings settings(AudioRenderCategory::Media);
		CreateAudioGraphResult result = co_await AudioGraph::CreateAsync(settings);

		if (result.Status() != AudioGraphCreationStatus::Success)
		{
			// Cannot create graph
			CoreApplication::Exit();
			return;
		}

		s_audioGraph = result.Graph();
		s_audioGraphState = AudioGraphState::Initialized;
	});
}

DeviceInfo HolofunkAudioGraph::HolofunkAudioGraph_GetDefaultRenderDeviceInfo()
{
	return DeviceInfo(nullptr, nullptr);
}

// TODO: really really need a real graph node store
AudioDeviceOutputNode s_deviceOutputNode{ nullptr };

void HolofunkAudioGraph::HolofunkAudioGraph_CreateAudioGraphAsync(DeviceInfo outputDevice)
{
	Contract::Requires(s_audioGraphState == AudioGraphState::Initialized);

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
		s_audioGraphState = AudioGraphState::Created;
	});
}

void HolofunkAudioGraph::HolofunkAudioGraph_StartAudioGraphAsync()
{
	Contract::Requires(s_audioGraphState == AudioGraphState::Created);

	s_audioGraph.Start();

	s_audioGraphState = AudioGraphState::Running;
}

void HolofunkAudioGraph::HolofunkAudioGraph_PlayUserSelectedSoundFileAsync()
{
	create_task([]() -> IAsyncAction
	{
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
	});
}

void HolofunkAudioGraph::HolofunkAudioGraph_DestroyAudioGraphAsync()
{
}
