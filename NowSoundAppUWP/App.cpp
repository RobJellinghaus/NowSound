// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
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

struct App : ApplicationT<App>
{
    const std::wstring AudioGraphStateString = L"Audio graph state: ";

    std::wstring StateLabel(NowSoundGraph_State state)
    {
        switch (state)
        {
        case NowSoundGraph_State::Uninitialized: return L"Uninitialized";
        case NowSoundGraph_State::Initialized: return L"Initialized";
        case NowSoundGraph_State::Created: return L"Created";
        case NowSoundGraph_State::Running: return L"Running";
        case NowSoundGraph_State::InError: return L"InError";
        default: Check(false); // Unknown graph state; should be impossible
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
        bool stateIsSame;
        // While the state isn't as expected yet, and we haven't reached timeoutTime, keep ticking.
        while (!(stateIsSame = expectedState == NowSoundGraph::NowSoundGraph_GetGraphState())
            && winrt::clock::now() < timeoutTime)
        {
            // wait in intervals of 1/100 sec
            co_await resume_after(TimeSpan((int)(TicksPerSecond * 0.01f)));
        }

        UpdateStateLabel();

        return stateIsSame;
    }

    void OnLaunched(LaunchActivatedEventArgs const&)
    {
        _textBlock1 = TextBlock();
        _textBlock1.Text(AudioGraphStateString);
        _textBlock2 = TextBlock();
        _textBlock2.Text(L"");

        _button1 = Button();
        _button1.Content(IReference<hstring>(L"Play Something"));

        _button1.Click([&](IInspectable const&, RoutedEventArgs const&)
        {
            NowSoundGraph::NowSoundGraph_PlayUserSelectedSoundFileAsync();
        });

        Window xamlWindow = Window::Current();

        StackPanel stackPanel = StackPanel();
        stackPanel.Children().Append(_textBlock1);
        stackPanel.Children().Append(_textBlock2);
        stackPanel.Children().Append(_button1);

        xamlWindow.Content(stackPanel);
        xamlWindow.Activate();

        // and here goes
        NowSoundGraph::NowSoundGraph_InitializeAsync();

        /*
        Compositor compositor;
        SpriteVisual visual = compositor.CreateSpriteVisual();
        Rect bounds = window.Bounds();
        visual.Size({ bounds.Width, bounds.Height });
        _target = compositor.CreateTargetForCurrentView();
        _target.Root(visual);

        window.PointerPressed([=](auto && ...)
        {
            static bool playing = true;
            playing = !playing;

            if (playing)
            {
                graph.Start();
            }
            else
            {
                graph.Stop();
            }
        });

        window.SizeChanged([=](auto &&, WindowSizeChangedEventArgs const & args)
        {
            visual.Size(args.Size());
        });
        */
        Async();
    }

    fire_and_forget Async()
    {
        apartment_context ui_thread;
        _uiThread = ui_thread;

        co_await resume_background();
        // wait only one second (and hopefully much less) for graph to become initialized.
        // 1000 second timeout is for early stage debugging.
        co_await WaitForGraphState(NowSound::NowSoundGraph_State::Initialized, winrt::clock::now() + timeSpanFromSeconds(1000));

        co_await resume_background();
        NowSound_DeviceInfo deviceInfo = NowSoundGraph::NowSoundGraph_GetDefaultRenderDeviceInfo();

        NowSoundGraph::NowSoundGraph_CreateAudioGraphAsync(deviceInfo);

        co_await WaitForGraphState(NowSoundGraph_State::Created, winrt::clock::now() + timeSpanFromSeconds(1000));

        NowSound_GraphInfo graphInfo = NowSoundGraph::NowSoundGraph_GetGraphInfo();

        co_await _uiThread;
        std::wstringstream wstr;
        wstr << L"Latency in samples: " << graphInfo.LatencyInSamples << " | Samples per quantum: " << graphInfo.SamplesPerQuantum;
        _textBlock2.Text(wstr.str());
        co_await resume_background();

        NowSoundGraph::NowSoundGraph_StartAudioGraphAsync();

        co_await WaitForGraphState(NowSoundGraph_State::Running, winrt::clock::now() + timeSpanFromSeconds(1000));
    }

    TextBlock _textBlock1{ nullptr };
    TextBlock _textBlock2{ nullptr };
    Button _button1{ nullptr };
    // Button _button2{ nullptr };

    apartment_context _uiThread;
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    Application::Start([](auto &&) { make<App>(); });
}
