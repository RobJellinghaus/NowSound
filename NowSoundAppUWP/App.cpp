// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "Check.h"
#include "NowSoundLib.h"

#include <string>
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
    // - Initially, a "Track #1: Uninitialized" button is visible.
    // - When clicked, this turns to "Track #1: Recording", and a new track begins recording.
    // - When *that* is clicked, it turns to "Track #1: FinishRecording" and then "Track #1: Looping",
    // and the track starts looping.
    // - Also, a new "Track #2: Uninitialized" button appears, with the same behavior.
    // 
    // Result: the app is effectively a simple live looper capable of looping N tracks.

    // There is one TrackButton per recorded track, plus one more to allow recording a new track.
    // Note that every method in TrackButton() expects to be called on the UI context.
    struct TrackButton
    {
        App* _app;
        int _trackNumber;
        TrackId _trackId;
        NowSoundTrackState _trackState;
        Button _button;
        TextBlock _textBlock;
        std::wstring _label;
        int64_t _recordingStartTime;

        void UpdateUI()
        {
            std::wstringstream wstr{};
            wstr << L" Track # " << _trackNumber << L": " << _label;
            hstring hstr{};
            hstr = wstr.str();
            _button.Content(IReference<hstring>(hstr));

            wstr = std::wstringstream{};
            if (_trackId != TrackId::Undefined)
            {
                NowSoundTrackInfo trackTimeInfo = NowSoundTrack_Info(_trackId);
                wstr.precision(3);
                wstr << L" | Start (beats): " << trackTimeInfo.StartTimeInBeats
                    << L" | Duration (beats): " << trackTimeInfo.DurationInBeats
                    << L" | Current beat: " << trackTimeInfo.LocalClockBeat
					<< L" | Last sample time: " << trackTimeInfo.LastSampleTime
                    << L" | Avg (r.s.): " << (int)trackTimeInfo.AverageRequiredSamples
                    << L" | Min (samp./quant.): " << (int)trackTimeInfo.MinimumTimeSinceLastQuantum
                    << L" | Max (s/q): " << (int)trackTimeInfo.MaximumTimeSinceLastQuantum
                    << L" | Avg (s/q): " << (int)trackTimeInfo.AverageTimeSinceLastQuantum
                    ;
            }
            else
            {
                wstr << L"";
            }
            _textBlock.Text(wstr.str());
        }

        // Update this track button.  If this track button just started looping, then make and return
        // a new (uninitialized) track button.
        std::unique_ptr<TrackButton> Update()
        {
            NowSoundTrackState currentState{};
            if (_trackId != TrackId::Undefined)
            {
                currentState = NowSoundTrack_State(_trackId);
            }

            std::unique_ptr<TrackButton> returnValue{};
            if (currentState != _trackState)
            {
                switch (currentState)
                {
                case NowSoundTrackState::TrackUninitialized:
                    _label = L"Uninitialized";
                    break;
                case NowSoundTrackState::TrackRecording:
                    _label = L"Recording";
                    break;
                case NowSoundTrackState::TrackLooping:
                    _label = L"Looping";
                    break;
                case NowSoundTrackState::TrackFinishRecording:
                    _label = L"FinishRecording";
                    break;
                }

                _trackState = currentState;
                if (currentState == NowSoundTrackState::TrackLooping)
                {
                    returnValue = std::unique_ptr<TrackButton>{new TrackButton{_app}};
                }
            }

            UpdateUI();

            return returnValue;
        }

        void HandleClick()
        {
            if (_trackState == NowSoundTrackState::TrackUninitialized)
            {
                // we haven't started recording yet; time to do so!
                _trackId = NowSoundGraph_CreateRecordingTrackAsync();
                // don't initialize _trackState; that's Update's job.
                // But do find out what time it is.
                NowSoundTimeInfo timeInfo = NowSoundGraph_GetTimeInfo();
                _recordingStartTime = timeInfo.TimeInSamples;
            }
            else if (_trackState == NowSoundTrackState::TrackRecording)
            {
                NowSoundTrack_FinishRecording(_trackId);
            }
        }

        TrackButton(App* app)
            : _app{ app },
            _trackNumber{ _nextTrackNumber++ },
            _trackId{ TrackId::Undefined },
            _button{ Button() },
            _textBlock{ TextBlock() },
            _label{ L"Uninitialized" },
            _trackState{NowSoundTrackState::TrackUninitialized}
        {
            UpdateUI();

            StackPanel trackPanel{};
            trackPanel.Orientation(Windows::UI::Xaml::Controls::Orientation::Horizontal);
            trackPanel.Children().Append(_button);
            trackPanel.Children().Append(_textBlock);
            app->_stackPanel.Children().Append(trackPanel);

            _button.Click([this](IInspectable const&, RoutedEventArgs const&)
            {
                HandleClick();
            });
        }

        // don't allow these to be copied ever
        TrackButton(TrackButton& other) = delete;
        TrackButton(TrackButton&& other) = delete;
    };

    // Label string.
    const std::wstring AudioGraphStateString = L"Audio graph state: ";

    TextBlock _textBlockGraphStatus{ nullptr };
    TextBlock _textBlockGraphInfo{ nullptr };
    TextBlock _textBlockTimeInfo{ nullptr };

    StackPanel _stackPanel{ nullptr };

    static int _nextTrackNumber;

    // The per-track UI controls.
    // This is only ever modified (and even traversed) from the UI context, so does not need to be locked.
    std::vector<std::unique_ptr<TrackButton>> _trackButtons{};

    // The apartment context of the UI thread; must co_await this before updating UI
    // (and must thereafter switch out of UI context ASAP, for liveness).
    apartment_context _uiThread;

    std::wstring StateLabel(NowSoundGraphState state)
    {
        switch (state)
        {
        case NowSoundGraphState::GraphUninitialized: return L"Uninitialized";
        case NowSoundGraphState::GraphInitialized: return L"Initialized";
        case NowSoundGraphState::GraphCreated: return L"Created";
        case NowSoundGraphState::GraphRunning: return L"Running";
        case NowSoundGraphState::GraphInError: return L"InError";
        default: { Check(false); return L""; } // Unknown graph state; should be impossible
        }
    }

    // Update the state label.
    // Must be on UI context.
    void UpdateStateLabel()
    {
        std::wstring str(AudioGraphStateString);
        str.append(StateLabel(NowSoundGraph_GetGraphState()));
        _textBlockGraphStatus.Text(str);
    }

    // Wait until the graph state becomes the expected state, or timeoutTime is reached.
    // Must be on background context (all waits should always be in background).
    std::future<bool> WaitForGraphState(NowSoundGraphState expectedState, TimeSpan timeout)
    {
        // TODO: ensure not on UI context.

        DateTime timeoutTime = winrt::clock::now() + timeout;

        // Polling wait is inferior to callbacks, but the Unity model is all about polling (aka realtime game loop),
        // so we use polling in this example -- and to determine how it actually works in modern C++.
        NowSoundGraphState currentState = NowSoundGraph_GetGraphState();
        // While the state isn't as expected yet, and we haven't reached timeoutTime, keep ticking.
        while (expectedState != NowSoundGraph_GetGraphState()
            && winrt::clock::now() < timeoutTime)
        {
            // wait in intervals of 1/1000 sec
            co_await resume_after(TimeSpan((int)(TicksPerSecond * 0.001f)));

            currentState = NowSoundGraph_GetGraphState();
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
        _textBlockGraphStatus = TextBlock();
        _textBlockGraphStatus.Text(AudioGraphStateString);
        _textBlockGraphInfo = TextBlock();
        _textBlockGraphInfo.Text(L"");
        _textBlockTimeInfo = TextBlock();
        _textBlockTimeInfo.Text(L"");

        Window xamlWindow = Window::Current();

        _stackPanel = StackPanel();
        _stackPanel.Children().Append(_textBlockGraphStatus);
        _stackPanel.Children().Append(_textBlockGraphInfo);
        _stackPanel.Children().Append(_textBlockTimeInfo);

        xamlWindow.Content(_stackPanel);
        xamlWindow.Activate();

        LaunchedAsync();
    }

    // Update all the track buttons.
    // Must be called on UI context.
    void UpdateButtons()
    {
        std::vector<std::unique_ptr<TrackButton>> newTrackButtons{};
        for (auto& button : _trackButtons)
        {
            std::unique_ptr<TrackButton> newButtonOpt = button->Update();
            if (newButtonOpt != nullptr)
            {
                newTrackButtons.push_back(std::move(newButtonOpt));
            }
        }
        for (auto& newButton : newTrackButtons)
        {
            _trackButtons.push_back(std::move(newButton));
        }
    }

    // loop forever, updating the buttons
    IAsyncAction UpdateLoop()
    {
        while (true)
        {
            // always wait in the background
            co_await resume_background();

            // wait in intervals of 1/100 sec
            co_await resume_after(TimeSpan((int)(TicksPerSecond * 0.01)));

            // switch to UI thread to update buttons and time info
            co_await _uiThread;

            // update time info
            NowSoundTimeInfo timeInfo = NowSoundGraph_GetTimeInfo();
            std::wstringstream wstr;
            wstr << L"Time (in audio samples): " << timeInfo.TimeInSamples
                << L" | Beat: " << timeInfo.BeatInMeasure
                << L" | Total beats: " << timeInfo.ExactBeat;
            _textBlockTimeInfo.Text(wstr.str());

            // update all buttons
            UpdateButtons();
        }
    }
};

int App::_nextTrackNumber{ 1 };

fire_and_forget App::LaunchedAsync()
{
    apartment_context ui_thread{};
    _uiThread = ui_thread;

    // and here goes
    NowSoundGraph_InitializeAsync();

    co_await resume_background();
    // wait only one second (and hopefully much less) for graph to become initialized.
    // 1000 second timeout is for early stage debugging.
    const int timeoutInSeconds = 1000;
    co_await WaitForGraphState(NowSound::NowSoundGraphState::GraphInitialized, timeSpanFromSeconds(timeoutInSeconds));

    NowSoundDeviceInfo deviceInfo = NowSoundGraph_GetDefaultRenderDeviceInfo();

    NowSoundGraph_CreateAudioGraphAsync(/*deviceInfo*/); // TODO: actual output device selection

    co_await WaitForGraphState(NowSoundGraphState::GraphCreated, timeSpanFromSeconds(timeoutInSeconds));

    NowSoundGraphInfo graphInfo = NowSoundGraph_GetGraphInfo();

    co_await _uiThread;
    std::wstringstream wstr;
    wstr << L"Latency in samples: " << graphInfo.LatencyInSamples << " | Samples per quantum: " << graphInfo.SamplesPerQuantum;
    _textBlockGraphInfo.Text(wstr.str());
    co_await resume_background();

    NowSoundGraph_StartAudioGraphAsync();

    co_await WaitForGraphState(NowSoundGraphState::GraphRunning, timeSpanFromSeconds(timeoutInSeconds));

    co_await _uiThread;

    // let's create our first TrackButton!
    _trackButtons.push_back(std::unique_ptr<TrackButton>(new TrackButton(this)));

    // and start our update loop!  Strangely, don't seem to need to await this....
    // TODO: uncomment
    co_await resume_background();
    co_await UpdateLoop();
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    Application::Start([](auto &&) { make<App>(); });
}
