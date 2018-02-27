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

    // Label string.
    const std::wstring AudioGraphStateString = L"Audio graph state: ";

    TextBlock _textBlock1{ nullptr };
    TextBlock _textBlock2{ nullptr };

    StackPanel _stackPanel{ nullptr };

    static int _nextTrackNumber;

    struct TrackButton
    {
        App* _app;
        int _trackNumber;
        TrackId _trackId;
        NowSoundTrack_State _trackState;
        Button _button;

        void HandleClick()
        {
            if (_trackId == -1)
            {
                // we haven't started recording yet; time to do so!
                // ... soon.
            }
        }

        TrackButton(App* app)
            : _app{ app },
            _trackNumber{ _nextTrackNumber++ },
            _trackId{ -1 },
            _button{ Button() }
        {
            // TODO: ensure this is only called from within UI context
            _button.Content(IReference<hstring>(L"Start Recording Track #"));

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
            _button{ std::move(other._button) }
        { }
    };

    std::vector<TrackButton> _trackButtons{};

    apartment_context _uiThread;

    std::wstring StateLabel(NowSoundGraph_State state)
    {
        switch (state)
        {
        case NowSoundGraph_State::Uninitialized: return L"Uninitialized";
        case NowSoundGraph_State::Initialized: return L"Initialized";
        case NowSoundGraph_State::Created: return L"Created";
        case NowSoundGraph_State::Running: return L"Running";
        case NowSoundGraph_State::InError: return L"InError";
        default: { Check(false); return L""; } // Unknown graph state; should be impossible
        }
    }

    // Update the state label.
    // Transition to the UI thread to do so, and then return to calling context before exiting.
    IAsyncAction UpdateStateLabel()
    {
        apartment_context calling_context;

        co_await _uiThread;
        std::wstring str(AudioGraphStateString);
        str.append(StateLabel(NowSoundGraph::NowSoundGraph_GetGraphState()));
        _textBlock1.Text(str);

        co_await calling_context;
    }

    // Wait until the graph state becomes the expected state, or timeoutTime is reached.
    std::future<bool> WaitForGraphState(NowSoundGraph_State expectedState, DateTime timeoutTime)
    {
        // Polling wait is inferior to callbacks, but the Unity model is all about polling (aka realtime game loop),
        // so we use polling in this example -- and to determine how it actually works in modern C++.
        NowSoundGraph_State currentState = NowSoundGraph::NowSoundGraph_GetGraphState();
        // While the state isn't as expected yet, and we haven't reached timeoutTime, keep ticking.
        while (expectedState != NowSoundGraph::NowSoundGraph_GetGraphState()
            && winrt::clock::now() < timeoutTime)
        {
            // wait in intervals of 1/1000 sec
            co_await resume_after(TimeSpan((int)(TicksPerSecond * 0.001f)));

            currentState = NowSoundGraph::NowSoundGraph_GetGraphState();
        }

        UpdateStateLabel();

        return expectedState == currentState;
    }

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
        NowSoundGraph::NowSoundGraph_InitializeAsync();

        Async();
    }

    fire_and_forget Async()
    {
        apartment_context ui_thread;
        _uiThread = ui_thread;

        co_await resume_background();
        // wait only one second (and hopefully much less) for graph to become initialized.
        // 1000 second timeout is for early stage debugging.
        const int timeoutInSeconds = 1000;
        co_await WaitForGraphState(
            NowSound::NowSoundGraph_State::Initialized,
            winrt::clock::now() + timeSpanFromSeconds(timeoutInSeconds));

        co_await resume_background();
        NowSound_DeviceInfo deviceInfo = NowSoundGraph::NowSoundGraph_GetDefaultRenderDeviceInfo();

        NowSoundGraph::NowSoundGraph_CreateAudioGraphAsync(/*deviceInfo*/); // TODO: actual output device selection

        co_await WaitForGraphState(NowSoundGraph_State::Created, winrt::clock::now() + timeSpanFromSeconds(timeoutInSeconds));

        NowSound_GraphInfo graphInfo = NowSoundGraph::NowSoundGraph_GetGraphInfo();

        co_await _uiThread;
        std::wstringstream wstr;
        wstr << L"Latency in samples: " << graphInfo.LatencyInSamples << " | Samples per quantum: " << graphInfo.SamplesPerQuantum;
        _textBlock2.Text(wstr.str());
        co_await resume_background();

        NowSoundGraph::NowSoundGraph_StartAudioGraphAsync();

        co_await WaitForGraphState(NowSoundGraph_State::Running, winrt::clock::now() + timeSpanFromSeconds(timeoutInSeconds));

        co_await _uiThread;

        // let's create our first TrackButton!
        _trackButtons.push_back(TrackButton(this));
    }

};

int App::_nextTrackNumber{ 1 };

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    Application::Start([](auto &&) { make<App>(); });
}
