// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"

#include "NowSoundFrequencyTracker.h"
#include "NowSoundGraph.h"
#include "BaseAudioProcessor.h"

namespace NowSound
{
    // Interface type for setting the dry/wet level of a particular plugin.
    class DryWetAudio
    {
    public:
        // Get the current dry/wet level (integer 0 to 100).
        virtual int GetDryWetLevel() = 0;

        // Set the dry/wet level.
        virtual void SetDryWetLevel(int dryWetLevel) = 0;
    };
}

#pragma once
#pragma once
