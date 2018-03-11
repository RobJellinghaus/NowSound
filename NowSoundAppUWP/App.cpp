// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "Check.h"
#include "NowSoundLib.h"
#include <sstream>

using namespace NowSound;

using namespace std::chrono;
using namespace winrt;

using namespace Windows::ApplicationModel::Activation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::System;

const int TicksPerSecond = 10000000;

TimeSpan timeSpanFromSeconds(int seconds)
{
    // TimeSpan is in 100ns units
    return TimeSpan(seconds * TicksPerSecond);
}

// Simple application which exercises NowSoundLib, allowing test of basic looping.
struct App : ApplicationT<App>
{
    // The interaction model of this app is:
    // - Status text box gets updated with overall graph state.
    // - Initially, a "Start Recording Track #1" button is visible.
    // - When clicked, "Start Recording" turns to "Stop Recording & Start Looping Track #1", and a new track gets recorded.
    // - When the "Stop Recording..." button is clicked, it turns to "Pause Track #1", and the track starts looping.
    // - If the "Pause Loop #1" button is clicked, the track pauses, and the button turns to "Resume Looping Track #1".
    //   Also, a new "Start Recording Track #2" button appears, with the same behavior.
    // 
    // Result: the app is effectively a simple live looper capable of looping N tracks.

    // There is one TrackButton per recorded track, plus one more to allow recording a new track.
    struct TrackButton
    {
        App* _app;
        int _trackNumber;
        TrackId _trackId;
        NowSoundTrackState _trackState;
        Button _button;
        std::wstring _label;

        void UpdateLabel()
        {
            // TODO: called only from UI thread

            wstringstream wstr{};
            wstr << _label << " Track # " << _trackNumber;
            hstring hstr{};
            hstr = wstr.str();
            _button.Content(IReference<hstring>(hstr));
        }

        void Update()
        {
            // TODO: called only from UI context

            NowSoundTrackState currentState{ NowSoundTrackAPI::NowSoundTrack_State(_trackId) };
            if (currentState != _trackState)
            {
                if (currentState == NowSoundTrackState::Looping)
                {
                    _label = L"Mute Currently Looping";
                }

                UpdateLabel();
                _trackState = currentState;
            }
        }

        void HandleClick()
        {
            if (_trackId == -1)
            {
                // we haven't started recording yet; time to do so!
                _trackId = NowSoundGraphAPI::NowSoundGraph_CreateRecordingTrackAsync();
                // don't initialize _trackState; that's Update's job
                _label = L"Stop Recording";
            }
            else if (_trackState == NowSoundTrackState::Recording)
            {
                NowSoundTrackAPI::NowSoundTrack_FinishRecording(_trackId);
                _label = L"Finishing Recording Of";
            }
        }

        TrackButton(App* app)
            : _app{ app },
            _trackNumber{ _nextTrackNumber++ },
            _trackId{ -1 },
            _button{ Button() },
            _label{ L"Start Recording" }
        {
            UpdateLabel();

            app->_stackPanel.Children().Append(_button);

            _button.Click([&](IInspectable const&, RoutedEventArgs const&)
            {
                HandleClick();
            });
        }

        TrackButton(TrackButton&& other)
            : _app{ other._app },
            _trackNumber{ other._trackNumber },
            _trackId{ other._trackId },
            _trackState{ other._trackState },
            _button{ std::move(other._button) },
            _label{ std::move(other._label) }
        { }
    };

    // Label string.
    const std::wstring AudioGraphStateString = L"Audio graph state: ";

    TextBlock _textBlock1{ nullptr };
    TextBlock _textBlock2{ nullptr };

    StackPanel _stackPanel{ nullptr };

    static int _nextTrackNumber;

    std::vector<TrackButton> _trackButtons{};

    std::mutex _mutex{};

    apartment_context _uiThread;

    std::wstring StateLabel(NowSoundGraphState state)
    {
        switch (state)
        {
        case NowSoundGraphState::Uninitialized: return L"Uninitialized";
        case NowSoundGraphState::Initialized: return L"Initialized";
        case NowSoundGraphState::Created: return L"Created";
        case NowSoundGraphState::Running: return L"Running";
        case NowSoundGraphState::InError: return L"InError";
        default: { Check(false); return L""; } // Unknown graph state; should be impossible
        }
    }

    // Update the state label.  Must be on UI context.
    void UpdateStateLabel()
    {
        std::wstring str(AudioGraphStateString);
        str.append(StateLabel(NowSoundGraphAPI::NowSoundGraph_GetGraphState()));
        _textBlock1.Text(str);
    }

    // Wait until the graph state becomes the expected state, or timeoutTime is reached.
    // Must be called from background context (all waits should always be in background).
    std::future<bool> WaitForGraphState(NowSoundGraphState expectedState, TimeSpan timeout)
    {
        // TODO: ensure not on UI context.

        DateTime timeoutTime = winrt::clock::now() + timeout;

        // Polling wait is inferior to callbacks, but the Unity model is all about polling (aka realtime game loop),
        // so we use polling in this example -- and to determine how it actually works in modern C++.
        NowSoundGraphState currentState = NowSoundGraphAPI::NowSoundGraph_GetGraphState();
        // While the state isn't as expected yet, and we haven't reached timeoutTime, keep ticking.
        while (expectedState != NowSoundGraphAPI::NowSoundGraph_GetGraphState()
            && winrt::clock::now() < timeoutTime)
        {
            // wait in intervals of 1/1000 sec
            co_await resume_after(TimeSpan((int)(TicksPerSecond * 0.001f)));

            currentState = NowSoundGraphAPI::NowSoundGraph_GetGraphState();
        }

        // switch to UI thread to update state label, then back to background
        co_await _uiThread;
        UpdateStateLabel();
        co_await resume_background();

        return expectedState == currentState;
    }

    fire_and_forget LaunchedAsync();

    void OnLaunched(LaunchActivatedEventArgs const&)
    {
        _textBlock1 = TextBlock();
        _textBlock1.Text(AudioGraphStateString);
        _textBlock2 = TextBlock();
        _textBlock2.Text(L"");

        Window xamlWindow = Window::Current();

        _stackPanel = StackPanel();
        _stackPanel.Children().Append(_textBlock1);
        _stackPanel.Children().Append(_textBlock2);

        xamlWindow.Content(_stackPanel);
        xamlWindow.Activate();

        // and here goes
        NowSoundGraphAPI::NowSoundGraph_InitializeAsync();

        LaunchedAsync();
    }

    void UpdateButtons()
    {
        // Must be called on UI context.

        // Lock the _trackButtons array
        {
            std::lock_guard<std::mutex> guard(_mutex);

            for (auto& button : _trackButtons)
            {
                button.Update();
            }
        }
    }

    // loop forever, updating the buttons
    IAsyncAction UpdateLoop()
    {
        for (;;)
        {
            // always wait in the background
            co_await resume_background();

            // wait in intervals of 1 sec (TODO: decrease once stable)
            co_await resume_after(TimeSpan((int)(TicksPerSecond * 1)));

            // switch to UI thread to update buttons
            co_await _uiThread;
            UpdateButtons();
        }
    }
};

int App::_nextTrackNumber{ 1 };

fire_and_forget App::LaunchedAsync()
{
    apartment_context ui_thread{};
    _uiThread = ui_thread;

    co_await resume_background();
    // wait only one second (and hopefully much less) for graph to become initialized.
    // 1000 second timeout is for early stage debugging.
    const int timeoutInSeconds = 1000;
    co_await WaitForGraphState(NowSound::NowSoundGraphState::Initialized, timeSpanFromSeconds(timeoutInSeconds));

    NowSoundDeviceInfo deviceInfo = NowSoundGraphAPI::NowSoundGraph_GetDefaultRenderDeviceInfo();

    NowSoundGraphAPI::NowSoundGraph_CreateAudioGraphAsync(/*deviceInfo*/); // TODO: actual output device selection

    co_await WaitForGraphState(NowSoundGraphState::Created, timeSpanFromSeconds(timeoutInSeconds));

    NowSoundGraphInfo graphInfo = NowSoundGraphAPI::NowSoundGraph_GetGraphInfo();

    co_await _uiThread;
    std::wstringstream wstr;
    wstr << L"Latency in samples: " << graphInfo.LatencyInSamples << " | Samples per quantum: " << graphInfo.SamplesPerQuantum;
    _textBlock2.Text(wstr.str());
    co_await resume_background();

    NowSoundGraphAPI::NowSoundGraph_StartAudioGraphAsync();

    co_await WaitForGraphState(NowSoundGraphState::Running, timeSpanFromSeconds(timeoutInSeconds));

    co_await _uiThread;

    // let's create our first TrackButton!
    {
        std::lock_guard<std::mutex> guard(_mutex);
        _trackButtons.push_back(TrackButton(this));
    }

    // and start our update loop!  Strangely, don't seem to need to await this....
    // TODO: uncomment
    // UpdateLoop();
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    Application::Start([](auto &&) { make<App>(); });
}
