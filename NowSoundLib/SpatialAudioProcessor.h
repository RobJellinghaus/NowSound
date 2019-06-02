// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"

#include <string>
#include "NowSoundFrequencyTracker.h"
#include "NowSoundGraph.h"
#include "MeasurementAudioProcessor.h"

namespace NowSound
{
    // Expects one input channel and N output channels; applies appropriate spatialization (at the moment,
    // stereo panning only) and writes to all N output channels.
    class SpatialAudioProcessor : public MeasurementAudioProcessor
    {
        // current pan value; 0 = left, 0.5 = center, 1 = right
        float _pan;

        // is this currently muted?
        // if so, output audio is zeroed
        bool _isMuted;

    public:
        SpatialAudioProcessor(NowSoundGraph* graph, const std::wstring& name, float initialPan);

        // Expect channel 0 to have mono audio data; update all channels with FX-applied output.
        virtual void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;

        // True if this is muted.
        // 
        // Note that something can be in FinishRecording state but still be muted, if the user is fast!
        // Hence this is a separate flag, not represented as a NowSoundTrack_State.
        bool IsMuted() const;
        void IsMuted(bool isMuted);

        // Get and set the pan value for this track.
        float Pan() const;
        void Pan(float pan);

	protected: 
		static std::wstring MakeName(const wchar_t* label, int id)
		{
			std::wstringstream wstr;
			wstr << label << id;
			return wstr.str();
		}
    };
}
