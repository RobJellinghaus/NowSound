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
#include "NowSoundLib.h"

using namespace NowSound;

static AudioGraphState HolofunkAudioGraph::HolofunkAudioGraph_GetGraphState()
{
}

			// Initialize the audio graph subsystem such that device information can be queried.
			// Graph must be Uninitialized.  On completion, graph becomes Initialized.
			static void HolofunkAudioGraph_InitializeAsync();

			// Get the device info for the default render device.
			// Graph must not be Uninitialized or InError.
			static DeviceInfo HolofunkAudioGraph_GetDefaultRenderDeviceInfo();

			// Create the audio graph.
			// Graph must be Initialized.  On completion, graph becomes Created.
			void HolofunkAudioGraph_CreateAudioGraphAsync(DeviceInfo outputDevice);

			// Start the audio graph.
			// Graph must be Created.  On completion, graph becomes Started.
			void HolofunkAudioGraph_StartAudioGraphAsync();

			// Play a user-selected sound file.
			// Graph must be Started.
			void HolofunkAudioGraph_PlayUserSelectedSoundFileAsync();

			// Tear down the whole graph.
			// Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
			void HolofunkAudioGraph_DestroyAudioGraphAsync();
		};
	}
}


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
}


{
	AudioGraphSettings settings(AudioRenderCategory::Media);
	CreateAudioGraphResult result = co_await AudioGraph::CreateAsync(settings);

	if (result.Status() != AudioGraphCreationStatus::Success)
	{
		// Cannot create graph
		CoreApplication::Exit();
		return;
	}

	AudioGraph graph = result.Graph();
}
{
	// Create a device output node
	CreateAudioDeviceOutputNodeResult deviceOutputNodeResult = co_await graph.CreateDeviceOutputNodeAsync();

	if (deviceOutputNodeResult.Status() != AudioDeviceNodeCreationStatus::Success)
	{
		// Cannot create device output node
		CoreApplication::Exit();
		return;
	}

	AudioDeviceOutputNode deviceOutput = deviceOutputNodeResult.DeviceOutputNode();
}
{
	CreateAudioFileInputNodeResult fileInputResult = await graph.CreateFileInputNodeAsync(file);
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

	fileInput.AddOutgoingConnection(deviceOutput);
}

{
	graph.Start();
}
