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

static NowSoundGraph_State s_audioGraphState{ NowSoundGraph_State::Uninitialized };

static AudioGraph s_audioGraph{ nullptr };

NowSoundGraph_State NowSoundGraph::NowSoundGraph_GetGraphState()
{
	return s_audioGraphState;
}

void NowSoundGraph::NowSoundGraph_InitializeAsync()
{
	Contract::Requires(s_audioGraphState == NowSoundGraph_State::Uninitialized);
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
		s_audioGraphState = NowSoundGraph_State::Initialized;
	});
}

NowSound_DeviceInfo NowSoundGraph::NowSoundGraph_GetDefaultRenderDeviceInfo()
{
	return NowSound_DeviceInfo(nullptr, nullptr);
}

// TODO: really really need a real graph node store
AudioDeviceOutputNode s_deviceOutputNode{ nullptr };

void NowSoundGraph::NowSoundGraph_CreateAudioGraphAsync(NowSound_DeviceInfo outputDevice)
{
	Contract::Requires(s_audioGraphState == NowSoundGraph_State::Initialized);

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

void NowSoundGraph::NowSoundGraph_StartAudioGraphAsync()
{
	Contract::Requires(s_audioGraphState == NowSoundGraph_State::Created);

	s_audioGraph.Start();

	s_audioGraphState = NowSoundGraph_State::Running;
}

void NowSoundGraph::NowSoundGraph_PlayUserSelectedSoundFileAsync()
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

void NowSoundGraph::NowSoundGraph_DestroyAudioGraphAsync()
{
}
