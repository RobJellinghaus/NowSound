// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "NowSoundLib.h"

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

TimeSpan timeSpanFromSeconds(int seconds)
{
	// TimeSpan is in 100ns units
	return TimeSpan(seconds * 10000000);
}

struct App : ApplicationT<App>
{
	const std::wstring AudioGraphState = L"Audio graph state: ";

    void OnLaunched(LaunchActivatedEventArgs const&)
    {
		m_textBlock = TextBlock();
		m_textBlock.Text() = AudioGraphState;

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
	}

	TextBlock m_textBlock{ nullptr };
	Button m_button1{ nullptr };
	// Button m_button2{ nullptr };
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	Application::Start([](auto &&) { make<App>(); });
}
