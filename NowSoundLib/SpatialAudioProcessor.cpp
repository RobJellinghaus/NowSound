// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include "Clock.h"
#include "MagicConstants.h"
#include "SpatialAudioProcessor.h"

using namespace NowSound;
using namespace std;

SpatialAudioProcessor::SpatialAudioProcessor(NowSoundGraph* graph, const wstring& name, float initialVolume, float initialPan) 
    : BaseAudioProcessor(graph, name),
    _isMuted{ false },
    _volume{ initialVolume },
    _pan{ initialPan },
    _outputProcessor{ new MeasurementAudioProcessor(graph, MakeName(name, L" Output")) },
    _pluginInstances{},
    _pluginNodeIds{}
{}

bool SpatialAudioProcessor::IsMuted() const { return _isMuted; }
void SpatialAudioProcessor::IsMuted(bool isMuted) { _isMuted = isMuted; }

float SpatialAudioProcessor::Pan() const { return _pan; }
void SpatialAudioProcessor::Pan(float pan)
{
    Check(pan >= 0);
    Check(pan <= 1);

    _pan = pan;
}

float SpatialAudioProcessor::Volume() const { return _volume; }
void SpatialAudioProcessor::Volume(float volume)
{
    Check(volume >= 0);

    _volume = volume;
}

const double Pi = std::atan(1) * 4;

float clamp(float value, float absLimit)
{
    Check(absLimit > 0);
    if (value < 0)
    {
        return value < -absLimit ? -absLimit : value;
    }
    else
    {
        return value > absLimit ? absLimit : value;
    }
}

void SpatialAudioProcessor::processBlock(AudioBuffer<float>& audioBuffer, MidiBuffer& midiBuffer)
{
    Check(audioBuffer.getNumChannels() == 2);
    Check(getTotalNumOutputChannels() == 2);
    Check(getTotalNumInputChannels() == 1 || getTotalNumInputChannels() == 2);

    int numSamples = audioBuffer.getNumSamples();

    // all input data comes in channel 0
    float* outputBufferChannel0 = audioBuffer.getWritePointer(0);
    float* outputBufferChannel1 = audioBuffer.getWritePointer(1);

    // Use cosine panner for volume preservation.
    double angularPosition = _pan * Pi / 2;
    double leftCoefficient = std::cos(angularPosition);
    double rightCoefficient = std::sin(angularPosition);

    // Pan each mono sample, if we're not muted.
    for (int i = 0; i < numSamples; i++)
    {
        float value = _isMuted ? 0 : outputBufferChannel0[i];
        outputBufferChannel0[i] = clamp((float)(leftCoefficient * _volume * value), 1.0f);
        outputBufferChannel1[i] = clamp((float)(rightCoefficient * _volume * value), 1.0f);
    }
}

void SpatialAudioProcessor::SetNodeIds(juce::AudioProcessorGraph::NodeID inputNodeId, juce::AudioProcessorGraph::NodeID outputNodeId)
{
    SetNodeId(inputNodeId);
    OutputProcessor()->SetNodeId(outputNodeId);

    // now set up the connections here
    // TODO: add in effects when pre-creating them
    {
        Check(Graph()->JuceGraph().addConnection({ { inputNodeId, 0 }, { outputNodeId, 0 } }));
        Check(Graph()->JuceGraph().addConnection({ { inputNodeId, 1 }, { outputNodeId, 1 } }));
    }
}

void SpatialAudioProcessor::Delete()
{
    // first remove all the plugins
    for (AudioProcessorGraph::NodeID nodeId : _pluginNodeIds)
    {
        Graph()->JuceGraph().removeNode(nodeId);
    }
    // then remove the output
    Graph()->JuceGraph().removeNode(OutputProcessor()->NodeId());
    // finally, remove this node -- after this line, this object may be destructed, as the graph holds the only
    // strong node (and hence audioprocessor) references
    Graph()->JuceGraph().removeNode(NodeId());
}

PluginInstanceIndex SpatialAudioProcessor::AddPluginInstance(PluginId pluginId, ProgramId programId, int dryWet_0_100)
{
    Check(pluginId >= 1);
    Check(pluginId <= Graph()->PluginCount());
    Check(programId >= 1);
    Check(programId <= Graph()->PluginProgramCount(pluginId));

    {
        std::wstringstream obuf;
        obuf << L"AddPluginInstance pluginId " << (int)pluginId << L" programId " << (int)programId;
        Graph()->Log(obuf.str());
    }

    // ok here goes nothing!
    AudioProcessor* newPluginInstance = Graph()->CreatePluginProcessor(pluginId, programId);

    // create a node for it
    AudioProcessorGraph::Node::Ptr newNode = Graph()->JuceGraph().addNode(newPluginInstance);

    // and connect it up!
    // what is the most recent (e.g. end) plugin?  If none, then we're hooking to the input.
    AudioProcessorGraph::NodeID inputNodeId = NodeId();
    if (_pluginNodeIds.size() > 0)
    {
        inputNodeId = _pluginNodeIds[_pluginNodeIds.size() - 1];
    }
    AudioProcessorGraph::NodeID outputNodeId = OutputProcessor()->NodeId();

    // remove connections from inputNode to outputNode
    {
        Check(Graph()->JuceGraph().removeConnection({ { inputNodeId, 0 }, { outputNodeId, 0 } }));
        Check(Graph()->JuceGraph().removeConnection({ { inputNodeId, 1 }, { outputNodeId, 1 } }));
    }

    // add connections from inputNode to new plugin instance...
    {
        Check(Graph()->JuceGraph().addConnection({ { inputNodeId, 0 }, { newNode->nodeID, 0 } }));
        Check(Graph()->JuceGraph().addConnection({ { inputNodeId, 1 }, { newNode->nodeID, 1 } }));
    }

    // ...and from new plugin instance to outputNode
    {
        Check(Graph()->JuceGraph().addConnection({ { newNode->nodeID, 0 }, { outputNodeId, 0 } }));
        Check(Graph()->JuceGraph().addConnection({ { newNode->nodeID, 1 }, { outputNodeId, 1 } }));
    }

    NowSoundPluginInstanceInfo info;
    info.NowSoundPluginId = pluginId;
    info.NowSoundProgramId = programId;
    info.DryWet_0_100 = dryWet_0_100;

    // update our plugin instance tracking state
    {
        _pluginInstances.push_back(info);
        _pluginNodeIds.push_back(newNode->nodeID);
    }

    // this is an async update (if we weren't running JUCE in such a hacky way, we wouldn't need to know this)
    Graph()->JuceGraphChanged();

    {
        std::wstringstream wstr{};
        wstr << L"SpatialAudioProcessor::AddPluginProgramInstance(): new node " << newNode->nodeID.uid;
        Graph()->Log(wstr.str());
    }

    return (PluginInstanceIndex)_pluginNodeIds.size();
}


int SpatialAudioProcessor::GetPluginInstanceCount()
{
    return (int)_pluginInstances.size();
}

NowSoundPluginInstanceInfo SpatialAudioProcessor::GetPluginInstanceInfo(PluginInstanceIndex index)
{
    Check((int)index >= 1);
    Check((int)index <= _pluginInstances.size());

    return _pluginInstances.at((int)index - 1);
}

void SpatialAudioProcessor::SetPluginInstanceDryWet(PluginInstanceIndex pluginInstanceIndex, int32_t dryWet_0_100)
{

}

void SpatialAudioProcessor::DeletePluginInstance(PluginInstanceIndex pluginInstanceIndex)
{
    Check(pluginInstanceIndex >= 1);
    Check(pluginInstanceIndex <= _pluginNodeIds.size());

    AudioProcessorGraph::NodeID priorNodeId =
        pluginInstanceIndex == 1 
        ? NodeId()
        : _pluginNodeIds[pluginInstanceIndex - 2];

    AudioProcessorGraph::NodeID deletedNodeId = _pluginNodeIds[pluginInstanceIndex - 1];

    AudioProcessorGraph::NodeID subsequentNodeId =
        pluginInstanceIndex == _pluginNodeIds.size()
        ? OutputProcessor()->NodeId()
        : _pluginNodeIds[pluginInstanceIndex];

    // drop the node and all connections 
    Graph()->JuceGraph().removeNode(Graph()->JuceGraph().getNodeForId(deletedNodeId));

    // reconnect prior node to subsequent
    {
        Check(Graph()->JuceGraph().addConnection({ { priorNodeId, 0 }, { subsequentNodeId, 0 } }));
        Check(Graph()->JuceGraph().addConnection({ { priorNodeId, 1 }, { subsequentNodeId, 1 } }));
    }

    // and clean up state
    {
        _pluginInstances.erase(_pluginInstances.begin() + (pluginInstanceIndex - 1));
        _pluginNodeIds.erase(_pluginNodeIds.begin() + (pluginInstanceIndex - 1));
    }

    // and spam the log
    {
        std::wstringstream wstr{};
        wstr << L"SpatialAudioProcessor::DeletePluginInstance(): deleted node " << deletedNodeId.uid
            << L", connected prior node " << priorNodeId.uid
            << L" to subsequent node " << subsequentNodeId.uid
            << L"; remaining plugin node ID count " << _pluginNodeIds.size();
        Graph()->Log(wstr.str());
    }

    // this is an async update (if we weren't running JUCE in such a hacky way, we wouldn't need to know this)
    Graph()->JuceGraphChanged();
}
