// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "Check.h"
#include "NowSoundApp.h"
#include "NowSoundAppMagicNumbers.h"

#include <string>
#include <sstream>
#include <iomanip>

using namespace NowSound;

using namespace std::chrono;
using namespace winrt;

using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::System;

const int TicksPerSecond = 10000000;

TimeSpan timeSpanFromSeconds(int seconds)
{
    // TimeSpan is in 100ns units
    return TimeSpan(seconds * TicksPerSecond);
}

std::wstring NowSoundApp::StateLabel(NowSoundGraphState state)
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
void NowSoundApp::UpdateStateLabel()
{
    std::wstring str(AudioGraphStateString);
    str.append(StateLabel(NowSoundGraph_State()));
    _textBlockGraphStatus.Text(str);
}

    // Wait until the graph state becomes the expected state, or timeoutTime is reached.
    // Must be on background context (all waits should always be in background).
std::future<bool> NowSoundApp::WaitForGraphState(NowSoundGraphState expectedState, TimeSpan timeout)
{
    // TODO: ensure not on UI context.

    DateTime timeoutTime = winrt::clock::now() + timeout;

    // Polling wait is inferior to callbacks, but the Unity model is all about polling (aka realtime game loop),
    // so we use polling in this example -- and to determine how it actually works in modern C++.
    NowSoundGraphState currentState = NowSoundGraph_State();
    // While the state isn't as expected yet, and we haven't reached timeoutTime, keep ticking.
    while (expectedState != NowSoundGraph_State()
        && winrt::clock::now() < timeoutTime)
    {
        // wait in intervals of 1/1000 sec
        co_await resume_after(TimeSpan((int)(TicksPerSecond * 0.001f)));

        currentState = NowSoundGraph_State();
    }

    // switch to UI thread to update state label, then back to background
    co_await _uiThread;
    UpdateStateLabel();
    co_await resume_background();

    return expectedState == currentState;
}

void NowSoundApp::OnLaunched(LaunchActivatedEventArgs const&)
{
    _textBlockGraphStatus = TextBlock();
    _textBlockGraphStatus.Text(AudioGraphStateString);
    _textBlockGraphInfo = TextBlock();
    _textBlockGraphInfo.Text(L"");
    _textBlockTimeInfo = TextBlock();
    _textBlockTimeInfo.Text(L"");
    _inputDeviceSelectionStackPanel = StackPanel();

    Window xamlWindow = Window::Current();

    _stackPanel = StackPanel();
    _stackPanel.Children().Append(_inputDeviceSelectionStackPanel);
    _stackPanel.Children().Append(_textBlockGraphStatus);
    _stackPanel.Children().Append(_textBlockGraphInfo);
    _stackPanel.Children().Append(_textBlockTimeInfo);

    xamlWindow.Content(_stackPanel);
    xamlWindow.Activate();

    LaunchedAsync();
}

    // Update all the track buttons.
    // Must be called on UI context.
void NowSoundApp::UpdateButtons()
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
IAsyncAction NowSoundApp::UpdateLoop()
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
        NowSoundTimeInfo timeInfo = NowSoundGraph_TimeInfo();
        NowSoundInputInfo input1Info = NowSoundGraph_InputInfo(AudioInputId::AudioInput1);
        NowSoundInputInfo input2Info = NowSoundGraph_InputInfo(AudioInputId::AudioInput2);
        std::wstringstream wstr;
        wstr << L"Time (in audio samples): " << timeInfo.TimeInSamples
            << std::fixed << std::setprecision(2)
            << L" | Beat: " << timeInfo.BeatInMeasure
            << L" | Total beats: " << timeInfo.ExactBeat
            << L" | Input 1 volume: " << input1Info.Volume
            << L" | Input 2 volume: " << input2Info.Volume;
        _textBlockTimeInfo.Text(wstr.str());

        // update all buttons
        UpdateButtons();
    }
}

int NowSoundApp::_nextTrackNumber{ 1 };

int NowSoundApp::GetNextTrackNumber() { return _nextTrackNumber++; }

StackPanel NowSoundApp::GetStackPanel() { return _stackPanel; }

fire_and_forget NowSoundApp::LaunchedAsync()
{
    apartment_context ui_thread{};
    _uiThread = ui_thread;

    co_await resume_background();

    // and here goes
    NowSoundGraph_InitializeAsync();

    // wait only one second (and hopefully much less) for graph to become initialized.
    // 1000 second timeout is for early stage debugging.
    const int timeoutInSeconds = 1000;
    co_await WaitForGraphState(NowSound::NowSoundGraphState::GraphInitialized, timeSpanFromSeconds(timeoutInSeconds));

    // how many input devices?
    NowSoundGraphInfo info = NowSoundGraph_Info();

    NowSoundGraph_InitializeFFT(
        NowSoundAppMagicNumbers::OutputBinCount,
        NowSoundAppMagicNumbers::CentralFrequency,
        NowSoundAppMagicNumbers::OctaveDivisions,
        NowSoundAppMagicNumbers::CentralFrequencyBin,
        NowSoundAppMagicNumbers::FftBinSize);

    co_await _uiThread;

    // Fill out the list of input devices and require the user to select at least one.
    std::unique_ptr<std::vector<int>> checkedEntries{new std::vector<int>()};
    Button okButton = Button();
    okButton.IsEnabled(false);
    for (int i = 0; i < info.InputDeviceCount; i++)
    {
        // Two bounded character buffers.
        const int bufSize = 512;
        wchar_t idBuf[bufSize];
        wchar_t nameBuf[bufSize];
            
        NowSoundGraph_InputDeviceId(i, idBuf, bufSize);
        NowSoundGraph_InputDeviceName(i, nameBuf, bufSize);

        std::wstring idWStr{ idBuf };
        std::wstring nameWStr{ nameBuf };

        StackPanel nextRow = StackPanel();
        nextRow.Orientation(Orientation::Horizontal);

        int j = i;

        CheckBox box = CheckBox();
        box.Content(winrt::box_value(nameBuf));
        nextRow.Children().Append(box);
        box.Checked([this, j, okButton](IInspectable const&, RoutedEventArgs const&)
        {
            _checkedInputDevices.push_back(j);

            okButton.IsEnabled(_checkedInputDevices.size() > 0);
        });
        box.Unchecked([this, j, okButton](IInspectable const&, RoutedEventArgs const&)
        {
            _checkedInputDevices.erase(std::find(_checkedInputDevices.begin(), _checkedInputDevices.end(), j));

            okButton.IsEnabled(_checkedInputDevices.size() > 0);
        });

        _inputDeviceSelectionStackPanel.Children().Append(nextRow);
    }
    okButton.Content(winrt::box_value(L"OK"));
    okButton.Click([this](IInspectable const&, RoutedEventArgs const&)
    {
        for (int deviceIndex : _checkedInputDevices)
        {
            NowSoundGraph_InitializeDeviceInputs(deviceIndex);
        }

        // and hide the devices
        _inputDeviceSelectionStackPanel.Visibility(Visibility::Collapsed);

        // and go on with the flow
        InputDevicesSelectedAsync();
    });
    _inputDeviceSelectionStackPanel.Children().Append(okButton);
}

fire_and_forget NowSoundApp::InputDevicesSelectedAsync()
{
    const int timeoutInSeconds = 1000;
    
    NowSoundGraph_CreateAudioGraphAsync(/*deviceInfo*/); // TODO: actual output device selection

    co_await WaitForGraphState(NowSoundGraphState::GraphCreated, timeSpanFromSeconds(timeoutInSeconds));

    NowSoundGraphInfo graphInfo = NowSoundGraph_Info();

    co_await _uiThread;
    std::wstringstream wstr;
    wstr << L"Sample rate in hz: " << graphInfo.SampleRateHz
        << L" | Channel count: " << graphInfo.ChannelCount
        << L" | Bits per sample: " << graphInfo.BitsPerSample
        << L" | Latency in samples: " << graphInfo.LatencyInSamples
        << L" | Samples per quantum: " << graphInfo.SamplesPerQuantum;
    _textBlockGraphInfo.Text(wstr.str());
    co_await resume_background();

    NowSoundGraph_StartAudioGraphAsync();

    co_await WaitForGraphState(NowSoundGraphState::GraphRunning, timeSpanFromSeconds(timeoutInSeconds));

    co_await _uiThread;

    // let's create our first TrackButton!
    _trackButtons.push_back(std::unique_ptr<TrackButton>(new TrackButton(this)));

    // and start our update loop!
    co_await resume_background();
    co_await UpdateLoop();
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    Application::Start([](auto &&) { make<NowSoundApp>(); });
}
