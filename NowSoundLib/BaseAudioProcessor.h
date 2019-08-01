// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"

#include <string>
#include "NowSoundGraph.h"

namespace NowSound
{
    // Simple audio processor with empty implementations for almost everything but processBlock and getName.
    class BaseAudioProcessor : public juce::AudioProcessor
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

    private:
        // NowSoundGraph we're part of
        NowSoundGraph* _graph;

        // Our node ID in that graph if any; this gets assigned post-construction after the node gets added to
        // (and becomes owned by) the graph
        juce::AudioProcessorGraph::NodeID _nodeId;

        // log throttling counter, to limit the amount of logging we emit (too much is useless)
        int _logThrottlingCounter;

        // log counter, to count the number of (throttled) log messages we emit (this helps with sequencing)
        int _logCounter;

    public:
        // the max counter at which _logThrottlingCounter rolls over
        static const int LogThrottle = 1000;

        // The graph this processor is part of.
        NowSoundGraph* Graph() const { return _graph; }

        // The ID of this node in the JUCE graph which owns it.
        // This is the default value, until SetNodeId gets called post-construction.
        juce::AudioProcessorGraph::NodeID NodeId() { return _nodeId; }

        // Set the node ID after it has been added to the JUCE graph.
        // This allows the processor to get hold of its owning node, for graph reconnections.
        void SetNodeId(juce::AudioProcessorGraph::NodeID nodeId);

        virtual const String getName() const override { return String(_name.c_str()); }

    protected:
        // The name of this processor.
        const std::wstring _name;

        BaseAudioProcessor(NowSoundGraph* graph, const std::wstring& name);

        // Return true if it is appropriate to emit a log message (happens every MaxCounter calls to this method).
        bool CheckLogThrottle();

        int NextCounter() { return ++_logCounter; }
    };
}
