// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "Check.h"
#include "NowSoundLib.h"
#include "TrackButton.h"

#include <string>
#include <sstream>
#include <iomanip>

winrt::Windows::Foundation::TimeSpan timeSpanFromSeconds(int seconds);

// Simple application which exercises NowSoundLib, allowing test of basic looping.
class NowSoundApp : winrt::Windows::UI::Xaml::ApplicationT<NowSoundApp>
{
	// The interaction model of this app is:
	// - Status text box gets updated with overall graph state.
	// - Initially, a "Track #1: Uninitialized" button is visible.
	// - When clicked, this turns to "Track #1: Recording", and a new track begins recording.
	// - When *that* is clicked, it turns to "Track #1: FinishRecording" and then "Track #1: Looping",
	// and the track starts looping.
	// - Also, a new "Track #2: Uninitialized" button appears, with the same behavior.
	// 
	// Result: the app is effectively a simple live looper capable of looping N tracks.

	// Label string.
	const std::wstring AudioGraphStateString = L"Audio graph state: ";

	winrt::Windows::UI::Xaml::Controls::TextBlock _textBlockGraphStatus{ nullptr };
	winrt::Windows::UI::Xaml::Controls::TextBlock _textBlockGraphInfo{ nullptr };
	winrt::Windows::UI::Xaml::Controls::TextBlock _textBlockTimeInfo{ nullptr };

	winrt::Windows::UI::Xaml::Controls::StackPanel _stackPanel{ nullptr };

	winrt::Windows::UI::Xaml::Controls::StackPanel _inputDeviceSelectionStackPanel{ nullptr };

	std::vector<int> _checkedInputDevices{};

	static int _nextTrackNumber;

	// The per-track UI controls.
	// This is only ever modified (and even traversed) from the UI context, so does not need to be locked.
	std::vector<std::unique_ptr<TrackButton>> _trackButtons{};

	// The apartment context of the UI thread; must co_await this before updating UI
	// (and must thereafter switch out of UI context ASAP, for liveness).
	winrt::apartment_context _uiThread;

	std::wstring StateLabel(NowSound::NowSoundGraphState state);

	// Update the state label.
	// Must be on UI context.
	void UpdateStateLabel();

	// Wait until the graph state becomes the expected state, or timeoutTime is reached.
	// Must be on background context (all waits should always be in background).
	std::future<bool> WaitForGraphState(NowSound::NowSoundGraphState expectedState, winrt::Windows::Foundation::TimeSpan timeout);

	winrt::fire_and_forget LaunchedAsync();
	winrt::fire_and_forget InputDevicesSelectedAsync();

	void OnLaunched(winrt::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs const&);

	// Update all the track buttons.
	// Must be called on UI context.
	void UpdateButtons();

	// loop forever, updating the buttons
	winrt::Windows::Foundation::IAsyncAction UpdateLoop();

public:
	int GetNextTrackNumber();

	winrt::Windows::UI::Xaml::Controls::StackPanel StackPanel();
};

