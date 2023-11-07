// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"

#include "NowSoundFrequencyTracker.h"
#include "NowSoundGraph.h"
#include "BaseAudioProcessor.h"
#include "DryWetAudio.h"

namespace NowSound
{
    // Takes 4 input channels (0/1 = dry, 2/3 = wet) and mixes them using a DryWetMixer.
    class DryWetMixAudioProcessor : public BaseAudioProcessor, public DryWetAudio
    {
        // The dry/wet level, as an integer from 0 (dry) to 100 (wet).
        // Should really be a float but we can change that later.
        int dryWetLevel;

    public:
        DryWetMixAudioProcessor(NowSoundGraph* graph, const std::wstring& name);

        // Process the given buffer; use the number of output channels as the channel count.
        // This locks the info mutex.
        virtual void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;

        virtual int GetDryWetLevel();
        virtual void SetDryWetLevel(int dryWetLevel);
    };
}

#pragma once
