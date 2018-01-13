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

struct App : implements<App, IFrameworkViewSource, IFrameworkView>
{
    IFrameworkView CreateView()
    {
        return *this;
    }

    void Initialize(CoreApplicationView const &)
    {
    }

    void Load(hstring const&)
    {
    }

    void Uninitialize()
    {
    }

    void Run()
    {
        CoreWindow window = CoreWindow::GetForCurrentThread();
        window.Activate();

        CoreDispatcher dispatcher = window.Dispatcher();
        dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
    }

    void SetWindow(CoreWindow const & window)
    {
        m_activated = window.Activated(auto_revoke, { this, &App::OnActivated });
    }

    fire_and_forget OnActivated(CoreWindow window, WindowActivatedEventArgs)
    {
        m_activated.revoke();

        Compositor compositor;
        SpriteVisual visual = compositor.CreateSpriteVisual();
        Rect bounds = window.Bounds();
        visual.Size({ bounds.Width, bounds.Height });
        m_target = compositor.CreateTargetForCurrentView();
        m_target.Root(visual);

        FileOpenPicker picker;
        picker.SuggestedStartLocation(PickerLocationId::MusicLibrary);
        picker.FileTypeFilter().Append(L".wav");
        StorageFile file = co_await picker.PickSingleFileAsync();

        if (!file)
        {
            CoreApplication::Exit();
            return;
        }

		AudioGraphSettings settings(AudioRenderCategory::Media);
		CreateAudioGraphResult result = co_await AudioGraph::CreateAsync(settings);

		if (result.Status() != AudioGraphCreationStatus::Success)
		{
			// Cannot create graph
			CoreApplication::Exit();
			return;
		}

		AudioGraph graph = result.Graph();

		// Create a device output node
		CreateAudioDeviceOutputNodeResult deviceOutputNodeResult = co_await graph.CreateDeviceOutputNodeAsync();

		if (deviceOutputNodeResult.Status() != AudioDeviceNodeCreationStatus::Success)
		{
			// Cannot create device output node
			CoreApplication::Exit();
			return;
		}

		AudioDeviceOutputNode deviceOutput = deviceOutputNodeResult.DeviceOutputNode();

		CreateAudioFileInputNodeResult fileInputResult = co_await graph.CreateFileInputNodeAsync(file);
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

		graph.Start();

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
    }

    CoreWindow::Activated_revoker m_activated;
    CompositionTarget m_target{ nullptr };
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CoreApplication::Run(App());
}
