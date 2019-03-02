// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "Check.h"
#include "NowSoundApp.h"
#include "NowSoundAppMagicNumbers.h"
#include "NowSoundLib.h"
#include "TrackButton.h"

using namespace NowSound;
using namespace winrt;

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::System;

void TrackButton::RenderFrequencyBuffer(std::wstring& output)
{
	Check(output.size() == NowSoundAppMagicNumbers::OutputBinCount);

	// _frequencyBuffer is presumed to have been updated
	// first, get min/max of values
	double max = 0;
	float* frequencies = (float*)(_frequencyBuffer.get());
	for (int i = 0; i < NowSoundAppMagicNumbers::OutputBinCount; i++)
	{
		double value = frequencies[i];
		// drop out super tiny values -- experimentally values less than 1 are uninteresting.
		// This will make silence not have huge variance from tiny max values.
		if (value < 1)
		{
			continue;
		}

		max = value > max ? value : max;
	}

	// scale to the max, dude
	if (max == 0)
	{
		output.replace(0, std::wstring::npos, output.size(), L'0');
	}
	else
	{
		for (int i = 0; i < NowSoundAppMagicNumbers::OutputBinCount; i++)
		{
			double scaledValue = frequencies[i] / max;
			WCHAR digit = L'0' + (WCHAR)std::floor(scaledValue * 9);
			output[i] = digit;
		}
	}
}

void TrackButton::UpdateUI()
{
	std::wstringstream wstr{};
	wstr << L" Track # " << _trackNumber << L" (id " << _trackId << L"): " << _label;
	hstring hstr{};
	hstr = wstr.str();
	_button.Content(IReference<hstring>(hstr));

	wstr = std::wstringstream{};
	if (_trackId != TrackId::TrackIdUndefined)
	{
		NowSoundTrackInfo trackInfo = NowSoundTrack_Info(_trackId);

		NowSoundTrack_GetFrequencies(_trackId, _frequencyBuffer.get(), NowSoundAppMagicNumbers::OutputBinCount);
		RenderFrequencyBuffer(_frequencyOutputString);

		wstr << std::fixed << std::setprecision(2)
			<< L" | Start (beats): " << trackInfo.StartTimeInBeats
			<< L" | Duration (beats): " << trackInfo.DurationInBeats
			<< L" | Current beat: " << trackInfo.LocalClockBeat
			<< L" | Volume: " << trackInfo.Volume
			<< L" | Last sample time: " << trackInfo.LastSampleTime
			<< L" | Frequencies: " << _frequencyOutputString
			/*
			<< L" | Avg (r.s.): " << (int)trackInfo.AverageRequiredSamples
			<< L" | Min (samp./quant.): " << (int)trackInfo.MinimumTimeSinceLastQuantum
			<< L" | Max (s/q): " << (int)trackInfo.MaximumTimeSinceLastQuantum
			<< L" | Avg (s/q): " << (int)trackInfo.AverageTimeSinceLastQuantum
			*/
			;
	}
	else
	{
		wstr << L"";
	}
	_textBlock.Text(winrt::hstring(wstr.str()));
}

	// Update this track button.  If this track button just started looping, then make and return
	// a new (uninitialized) track button.
std::unique_ptr<TrackButton> TrackButton::Update()
{
	NowSoundTrackState currentState{};
	if (_trackId != TrackId::TrackIdUndefined)
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
			returnValue = std::unique_ptr<TrackButton>{ new TrackButton{ _app } };
		}
	}

	UpdateUI();

	return returnValue;
}

void TrackButton::HandleClick()
{
	if (_trackState == NowSoundTrackState::TrackUninitialized)
	{
		// we haven't started recording yet; time to do so!
		_trackId = NowSoundGraph_CreateRecordingTrackAsync((AudioInputId)(_combo.SelectedIndex() + 1));
		// don't initialize _trackState; that's Update's job.
		// But do find out what time it is.
		NowSoundTimeInfo graphInfo = NowSoundGraph_TimeInfo();
		_recordingStartTime = graphInfo.TimeInSamples;
		_combo.IsEnabled(false);
	}
	else if (_trackState == NowSoundTrackState::TrackRecording)
	{
		NowSoundTrack_FinishRecording(_trackId);
	}
}

TrackButton::TrackButton(NowSoundApp* app)
	: _app{ app },
	_trackNumber{ _app->GetNextTrackNumber() },
	_trackId{ TrackId::TrackIdUndefined },
	_button{ Button() },
	_combo{ ComboBox() },
	_textBlock{ TextBlock() },
	_label{ L"Uninitialized" },
	_trackState{ NowSoundTrackState::TrackUninitialized },
	_frequencyBuffer{ new float[NowSoundAppMagicNumbers::OutputBinCount] },
	_frequencyOutputString{} // fill constructor doesn't resolve correctly here

{
	_frequencyOutputString.resize(NowSoundAppMagicNumbers::OutputBinCount, L'0');
	UpdateUI();

	StackPanel trackPanel{};
	trackPanel.Orientation(winrt::Windows::UI::Xaml::Controls::Orientation::Horizontal);
	trackPanel.Children().Append(_button);
	trackPanel.Children().Append(_combo);
	trackPanel.Children().Append(_textBlock);
	app->GetStackPanel().Children().Append(trackPanel);

	_button.Click([this](winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		HandleClick();
	});

	// populate the combo box with one entry per audio input
	NowSoundTimeInfo timeInfo = NowSoundGraph_TimeInfo();
	for (int i = 0; i < timeInfo.AudioInputCount; i++)
	{
		std::wstringstream wstr{};
		wstr << L"Input " << i;
		_combo.Items().Append(winrt::box_value(wstr.str()));
	}
	_combo.SelectedIndex(0);
}

