// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "Contract.h"
#include "NowSoundLib.h"

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

	return stateIsSame;
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
		default: Contract::Assert(false, L"Unknown graph state; should be impossible");
		}
	}

    void OnLaunched(LaunchActivatedEventArgs const&)
    {
		m_textBlock = TextBlock();
		m_textBlock.Text() = AudioGraphStateString;

		m_button1 = Button();
		m_button1.Content(IReference<hstring>(L"Play Something"));

		m_button1.Click([&](IInspectable const& sender, RoutedEventArgs const&)
		{
			m_button1.Content(IReference<hstring>(L"Psych!"));
		});

		Window xamlWindow = Window::Current();

		StackPanel stackPanel = StackPanel();
		stackPanel.Children().Append(m_button1);
		//stackPanel.Children().Append(m_button2);

		xamlWindow.Content(stackPanel);
		xamlWindow.Activate();

		// and here goes
		NowSoundGraph::NowSoundGraph_InitializeAsync();

		/*
        Compositor compositor;
        SpriteVisual visual = compositor.CreateSpriteVisual();
        Rect bounds = window.Bounds();
        visual.Size({ bounds.Width, bounds.Height });
        m_target = compositor.CreateTargetForCurrentView();
        m_target.Root(visual);

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

		co_await resume_background();
		// wait only one second (and hopefully much less) for graph to become initialized.
		co_await WaitForGraphState(NowSound::NowSoundGraph_State::Initialized, winrt::clock::now() + timeSpanFromSeconds(1));

		co_await ui_thread;
		std::wstring str(AudioGraphStateString);
		str.append(StateLabel(NowSoundGraph_State::Initialized));
		m_textBlock.Text() = str;
	}

	TextBlock m_textBlock{ nullptr };
	Button m_button1{ nullptr };
	// Button m_button2{ nullptr };
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	Application::Start([](auto &&) { make<App>(); });
}
