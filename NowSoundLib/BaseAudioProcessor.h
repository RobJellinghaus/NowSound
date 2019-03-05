#pragma once

#include "stdafx.h"

#include "NowSoundGraph.h"

namespace NowSound
{
    // Simple audio processor with empty implementations for almost everything but processBlock and getName.
    class BaseAudioProcessor : juce::AudioProcessor
    {
        // Inherited via AudioProcessor
        
        // Deliberately do not implement getName(); it helps debugging for derived types to do this.
        // virtual const String getName() const override;

        virtual void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
        virtual void releaseResources() override;

        // Deliberately do not implement this processBlock; that's just about all a derived class has to do.
        // virtual void processBlock(AudioBuffer<float>& buffer, MidiBuffer & midiMessages) override;

        virtual double getTailLengthSeconds() const override;
        virtual bool acceptsMidi() const override;
        virtual bool producesMidi() const override;
        virtual AudioProcessorEditor * createEditor() override;
        virtual bool hasEditor() const override;
        virtual int getNumPrograms() override;
        virtual int getCurrentProgram() override;
        virtual void setCurrentProgram(int index) override;
        virtual const String getProgramName(int index) override;
        virtual void changeProgramName(int index, const String & newName) override;
        virtual void getStateInformation(juce::MemoryBlock & destData) override;
        virtual void setStateInformation(const void * data, int sizeInBytes) override;
    };
}
