// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include <future>

using namespace std::chrono;
using namespace winrt;

using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Render;
using namespace Windows::System;

namespace NowSound
{
	// All external methods here are static and use C linkage, for P/Invokability.
	// AudioGraph object references are passed by integer ID; lifecycle is documented
	// per-API.
	// Callbacks are implemented by passing statically invokable callback hooks
	// together with callback IDs.
	extern "C"
	{
		struct DeviceInfo
		{
			LPWSTR Id;
			LPWSTR Name;

			// Construct a DeviceInfo; it will directly reference the given dictionary (no copying).
			// Note that this does *not* own the strings; these must be owned elsewhere.
			DeviceInfo(LPWSTR id, LPWSTR name)
			{
				Id = id;
				Name = name;
			}
		};

		enum AudioGraphState
		{
			// InitializeAsync() has not yet been called.
			Uninitialized,

			// Some error has occurred; GetLastError() will have details.
			InError,

			// A gap in incoming audio data was detected; this should ideally never happen.
			// NOTYET: Discontinuity,

			// InitializeAsync() has completed; devices can now be queried.
			Initialized,

			// CreateAudioGraphAsync() has completed; other methods can now be called.
			Created,

			// The audio graph has been started and is running.
			Running,

			// The audio graph has been stopped.
			// NOTYET: Stopped,
		};

		// The ID of a NowSound track; avoids issues with marshaling object references.
		// Note that 0 is a valid value.
		// NOTYET: typedef size_t TrackId;

		// Operations on the audio graph as a whole.
		// There is a single "static" audio graph defined here; multiple audio graphs are not supported.
		// All async methods document the state the graph must be in when called, and the state the graph
		// transitions to on completion.
		class HolofunkAudioGraph
		{
		public:
			// Get the current state of the audio graph; intended to be efficiently pollable by the client.
			// This is the only method that may be called in any state whatoever.
			static AudioGraphState HolofunkAudioGraph_GetGraphState();

			// Initialize the audio graph subsystem such that device information can be queried.
			// Graph must be Uninitialized.  On completion, graph becomes Initialized.
			static void HolofunkAudioGraph_InitializeAsync();

			// Get the device info for the default render device.
			// Graph must not be Uninitialized or InError.
			static DeviceInfo HolofunkAudioGraph_GetDefaultRenderDeviceInfo();

			// Create the audio graph.
			// Graph must be Initialized.  On completion, graph becomes Created.
			static void HolofunkAudioGraph_CreateAudioGraphAsync(DeviceInfo outputDevice);

			// Start the audio graph.
			// Graph must be Created.  On completion, graph becomes Started.
			static void HolofunkAudioGraph_StartAudioGraphAsync();

			// Play a user-selected sound file.
			// Graph must be Started.
			static void HolofunkAudioGraph_PlayUserSelectedSoundFileAsync();

			// Tear down the whole graph.
			// Graph may be in any state other than InError. On completion, graph becomes Uninitialized.
			static void HolofunkAudioGraph_DestroyAudioGraphAsync();

		private:
			static IAsyncAction InitializeAsyncImpl();
		};
	}
}
