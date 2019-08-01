// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"
#include "Check.h"
#include "NowSoundLib.h"

#include <string>
#include <sstream>
#include <iomanip>

class NowSoundApp;

// There is one TrackButton per recorded track, plus one more to allow recording a new track.
// Note that every method in TrackButton() expects to be called on the UI context.
class TrackButton
{
    NowSoundApp* _app;
    int _trackNumber;
    NowSound::TrackId _trackId;
    NowSound::NowSoundTrackState _trackState;
    winrt::Windows::UI::Xaml::Controls::Button _button;
    winrt::Windows::UI::Xaml::Controls::ComboBox _combo;
    winrt::Windows::UI::Xaml::Controls::TextBlock _textBlock;
    std::wstring _label;
    int64_t _recordingStartTime;
    std::unique_ptr<float> _frequencyBuffer;
    std::wstring _frequencyOutputString;

    void RenderFrequencyBuffer(std::wstring& output);

    void UpdateUI();

    void HandleClick();

    // don't allow these to be copied ever
    TrackButton(TrackButton& other) = delete;
    TrackButton(TrackButton&& other) = delete;

public:
    // Update this track button.  If this track button just started looping, then make and return
    // a new (uninitialized) track button.
    std::unique_ptr<TrackButton> Update();

    TrackButton(NowSoundApp* app);
};


