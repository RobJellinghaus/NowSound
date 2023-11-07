// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

using namespace std;

#include "Clock.h"
#include "MagicConstants.h"
#include "DryWetMixAudioProcessor.h"

using namespace NowSound;

DryWetMixAudioProcessor::DryWetMixAudioProcessor(NowSoundGraph* graph, const wstring& name)
    : BaseAudioProcessor(graph, name),
    dryWetLevel(0)
{}

void DryWetMixAudioProcessor::processBlock(AudioBuffer<float>& audioBuffer, MidiBuffer& midiBuffer)
{
    // temporary debugging code: see if processBlock is ever being called under Holofunk
    if (CheckLogThrottle())
    {
        std::wstringstream wstr{};
        wstr << getName() << L"::processBlock: count " << NextCounter();
        NowSoundGraph::Instance()->Log(wstr.str());
    }

    Check(audioBuffer.getNumChannels() == 4);

    int numSamples = audioBuffer.getNumSamples();

    // channels 0 and 1 are dry, channels 2 and 3 are wet
    const float* outputBufferChannel0 = audioBuffer.getReadPointer(0);
    const float* outputBufferChannel1 = audioBuffer.getReadPointer(1);
    const float* outputBufferChannel2 = audioBuffer.getReadPointer(2);
    const float* outputBufferChannel3 = audioBuffer.getReadPointer(3);

    // Pan each mono sample (and track its volume), if we're not muted.
    {
        for (int i = 0; i < numSamples; i++)
        {
            // we average the stereo channels when computing the histogram values to add
            float value0 = outputBufferChannel0[i];
            float value1 = outputBufferChannel1[i];

            float value2 = outputBufferChannel2[i];
            float value3 = outputBufferChannel3[i];

            float mix = (float)dryWetLevel / (float)100.0;

            audioBuffer.getWritePointer(0)[i] = (value0 * (1 - mix)) + (value2 * mix);
            audioBuffer.getWritePointer(1)[i] = (value1 * (1 - mix)) + (value3 * mix);
        }
    }
}

int DryWetMixAudioProcessor::GetDryWetLevel()
{
    return dryWetLevel;
}

void DryWetMixAudioProcessor::SetDryWetLevel(int dryWetLevel)
{
    this->dryWetLevel = dryWetLevel;
}
