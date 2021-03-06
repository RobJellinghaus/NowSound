// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"
#include "BaseAudioProcessor.h"
#include "NowSoundGraph.h"
#include <iostream>

NowSound::BaseAudioProcessor::BaseAudioProcessor(NowSoundGraph* graph, const std::wstring& name)
    : _graph{ graph },
    _name { name },
    _nodeId{}
{}

bool NowSound::BaseAudioProcessor::CheckLogThrottle()
{
    // TODO: revive if necessary... for now, always false
    /*
    int counter = _logThrottlingCounter;
    _logThrottlingCounter = ++_logThrottlingCounter % LogThrottle;
    return counter == 0;
    */
    return false;
}

void NowSound::BaseAudioProcessor::SetNodeId(juce::AudioProcessorGraph::NodeID nodeId)
{
    // Check that this node doesn't already have an ID and is actually getting one now.
    Check(_nodeId == juce::AudioProcessorGraph::NodeID{});
    Check(nodeId != juce::AudioProcessorGraph::NodeID{});

    // This will now be the node's ID forever.
    _nodeId = nodeId;
}

void NowSound::BaseAudioProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
}

void NowSound::BaseAudioProcessor::releaseResources()
{
}

double NowSound::BaseAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

bool NowSound::BaseAudioProcessor::acceptsMidi() const
{
    return false;
}

bool NowSound::BaseAudioProcessor::producesMidi() const
{
    return false;
}

AudioProcessorEditor * NowSound::BaseAudioProcessor::createEditor()
{
    return nullptr;
}

bool NowSound::BaseAudioProcessor::hasEditor() const
{
    return false;
}

int NowSound::BaseAudioProcessor::getNumPrograms()
{
    return 0;
}

int NowSound::BaseAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NowSound::BaseAudioProcessor::setCurrentProgram(int index)
{
}

const String NowSound::BaseAudioProcessor::getProgramName(int index)
{
    return String();
}

void NowSound::BaseAudioProcessor::changeProgramName(int index, const String & newName)
{
}

void NowSound::BaseAudioProcessor::getStateInformation(juce::MemoryBlock & destData)
{
}

void NowSound::BaseAudioProcessor::setStateInformation(const void * data, int sizeInBytes)
{
}
