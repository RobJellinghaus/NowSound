// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include "Clock.h"
#include "MagicConstants.h"
#include "SpatialAudioProcessor.h"
#include "DryWetAudio.h"
#include "DryWetMixAudioProcessor.h"

using namespace NowSound;
using namespace std;

SpatialAudioProcessor::SpatialAudioProcessor(NowSoundGraph* graph, const wstring& name, bool isMuted, float initialVolume, float initialPan) 
    : BaseAudioProcessor(graph, name),
    _isMuted{ false },
    _volume{ initialVolume },
    _pan{ initialPan },
    _outputProcessor{ new MeasurementAudioProcessor(graph, MakeName(name, L" Output")) },
    _pluginInstances{},
    _pluginNodeIds{},
    _dryWetNodeIds{}
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
    Check(getTotalNumInputChannels() == 0 || getTotalNumInputChannels() == 1 || getTotalNumInputChannels() == 2);

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
        outputBufferChannel0[i] = clamp((float)(leftCoefficient * _volume * value), 0.99f);
        outputBufferChannel1[i] = clamp((float)(rightCoefficient * _volume * value), 0.99f);
    }

    // And that's it! audioBuffer is good to go, ship it.
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
    AudioProcessorGraph::Node::Ptr newPluginNode = Graph()->JuceGraph().addNode(newPluginInstance);

    // And create the mixer too
    DryWetMixAudioProcessor* newDryWetMixInstance = new DryWetMixAudioProcessor(Graph(), L"DryWetMix");
    // Four input channels and two output channels
    newDryWetMixInstance->setPlayConfigDetails(4, 2, Graph()->Info().SampleRateHz, Graph()->Info().SamplesPerQuantum);
    newDryWetMixInstance->SetDryWetLevel(dryWet_0_100);
    AudioProcessorGraph::Node::Ptr newDryWetMixNode = Graph()->JuceGraph().addNode(newDryWetMixInstance);

    // and connect it up!
    // what is the most recent (e.g. end) node?  If none, then we're hooking to the input; otherwise
    // we're hooking to the tail DryWetMix node.
    AudioProcessorGraph::NodeID inputNodeId = NodeId();
    if (_dryWetNodeIds.size() > 0)
    {
        inputNodeId = _dryWetNodeIds[_dryWetNodeIds.size() - 1];
    }
    AudioProcessorGraph::NodeID outputNodeId = OutputProcessor()->NodeId();

    // remove connections from inputNode to outputNode
    {
        Check(Graph()->JuceGraph().removeConnection({ { inputNodeId, 0 }, { outputNodeId, 0 } }));
        Check(Graph()->JuceGraph().removeConnection({ { inputNodeId, 1 }, { outputNodeId, 1 } }));
    }

    // add connections from inputNode to new plugin instance...
    {
        Check(Graph()->JuceGraph().addConnection({ { inputNodeId, 0 }, { newPluginNode->nodeID, 0 } }));
        Check(Graph()->JuceGraph().addConnection({ { inputNodeId, 1 }, { newPluginNode->nodeID, 1 } }));
    }

    // ...and from inputNode to dry side of new drywet instance...
    {
        Check(Graph()->JuceGraph().addConnection({ { inputNodeId, 0 }, { newDryWetMixNode->nodeID, 0 } }));
        Check(Graph()->JuceGraph().addConnection({ { inputNodeId, 1 }, { newDryWetMixNode->nodeID, 1 } }));
    }

    // ...and from new plugin instance to wet side of new drywet instance...
    {
        Check(Graph()->JuceGraph().addConnection({ { newPluginNode->nodeID, 0 }, { newDryWetMixNode->nodeID, 2 } }));
        Check(Graph()->JuceGraph().addConnection({ { newPluginNode->nodeID, 1 }, { newDryWetMixNode->nodeID, 3 } }));
    }

    {
        std::wstringstream obuf;
        obuf << L"AddPluginInstance: adding connection from " << newDryWetMixNode->nodeID.uid << L" to " << outputNodeId.uid;
        AudioProcessorGraph::Node::Ptr dryWetMixNode = Graph()->JuceGraph().getNode(newDryWetMixNode->nodeID.uid);
        AudioProcessorGraph::Node::Ptr outputNode = Graph()->JuceGraph().getNode(outputNodeId.uid);
        bool canConnect0to0 = Graph()->JuceGraph().canConnect(dryWetMixNode.get(), 0, outputNode.get(), 0);
        bool canConnect1to1 = Graph()->JuceGraph().canConnect(dryWetMixNode.get(), 1, outputNode.get(), 1);
        obuf << L"AddPluginInstance: canConnect0to0: " << canConnect0to0 << L"; canConnect1to1: " << canConnect1to1;
        Graph()->Log(obuf.str());
    }

    // ...and from new drywet instance to output node
    {
        Check(Graph()->JuceGraph().addConnection({ { newDryWetMixNode->nodeID, 0 }, { outputNodeId, 0 } }));
        Check(Graph()->JuceGraph().addConnection({ { newDryWetMixNode->nodeID, 1 }, { outputNodeId, 1 } }));
    }

    NowSoundPluginInstanceInfo info;
    info.NowSoundPluginId = pluginId;
    info.NowSoundProgramId = programId;
    info.DryWet_0_100 = dryWet_0_100;

    // update our node tracking state
    {
        _pluginInstances.push_back(info);
        _pluginNodeIds.push_back(newPluginNode->nodeID);
        _dryWetNodeIds.push_back(newDryWetMixNode->nodeID);
    }

    // this is an async update (if we weren't running JUCE in such a hacky way, we wouldn't need to know this)
    Graph()->JuceGraphChanged();

    {
        std::wstringstream wstr{};
        wstr << L"SpatialAudioProcessor::AddPluginProgramInstance(): new plugin node " << newPluginNode->nodeID.uid << "; new drywet node " << newDryWetMixNode->nodeID.uid;
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

void SpatialAudioProcessor::SetPluginInstanceDryWet(PluginInstanceIndex index, int32_t dryWet_0_100)
{
    Check((int)index >= 1);
    Check((int)index <= _pluginInstances.size());
    Check(dryWet_0_100 >= 0);
    Check(dryWet_0_100 <= 100);

    auto dryWetNode = Graph()->JuceGraph().getNodeForId(_dryWetNodeIds[index - 1]);
    auto dryWetAudio = dynamic_cast<DryWetAudio*>(dryWetNode->getProcessor());
    dryWetAudio->SetDryWetLevel(dryWet_0_100);

    _pluginInstances[index - 1].DryWet_0_100 = dryWet_0_100;
}

void SpatialAudioProcessor::DeletePluginInstance(PluginInstanceIndex pluginInstanceIndex)
{
    Check(pluginInstanceIndex >= 1);
    Check(pluginInstanceIndex <= _pluginNodeIds.size());

    AudioProcessorGraph::NodeID priorNodeId =
        pluginInstanceIndex == 1 
        ? NodeId()
        : _dryWetNodeIds[pluginInstanceIndex - 2];

    AudioProcessorGraph::NodeID deletedPluginNodeId = _pluginNodeIds[pluginInstanceIndex - 1];
    AudioProcessorGraph::NodeID deletedDryWetNodeId = _dryWetNodeIds[pluginInstanceIndex - 1];

    bool wasLast = pluginInstanceIndex == _pluginNodeIds.size();

    // drop the node and all connections 
    Graph()->JuceGraph().removeNode(Graph()->JuceGraph().getNodeForId(deletedPluginNodeId));
    Graph()->JuceGraph().removeNode(Graph()->JuceGraph().getNodeForId(deletedDryWetNodeId));

    // reconnect prior node to subsequent
    if (wasLast)
    {
        Check(Graph()->JuceGraph().addConnection({ { priorNodeId, 0 }, { OutputProcessor()->NodeId(), 0 } }));
        Check(Graph()->JuceGraph().addConnection({ { priorNodeId, 1 }, { OutputProcessor()->NodeId(), 1 } }));

        // and spam the log
        {
            std::wstringstream wstr{};
            wstr << L"SpatialAudioProcessor::DeletePluginInstance(): deleted node " << deletedPluginNodeId.uid
                << L", connected prior node " << priorNodeId.uid
                << L" to output node " << OutputProcessor()->NodeId().uid;
            Graph()->Log(wstr.str());
        }
    }
    else
    {
        // need to connect it to inputs of both subsequent plugin and subsequent drywet.
        // they are already connected to each other and their subsequent outputs
        AudioProcessorGraph::NodeID subsequentPluginNodeId = _pluginNodeIds[pluginInstanceIndex];
        AudioProcessorGraph::NodeID subsequentDryWetNodeId = _dryWetNodeIds[pluginInstanceIndex];

        Check(Graph()->JuceGraph().addConnection({ { priorNodeId, 0 }, { subsequentPluginNodeId, 0 } }));
        Check(Graph()->JuceGraph().addConnection({ { priorNodeId, 1 }, { subsequentPluginNodeId, 1 } }));

        Check(Graph()->JuceGraph().addConnection({ { priorNodeId, 0 }, { subsequentDryWetNodeId, 0 } }));
        Check(Graph()->JuceGraph().addConnection({ { priorNodeId, 1 }, { subsequentDryWetNodeId, 1 } }));

        // and spam the log
        {
            std::wstringstream wstr{};
            wstr << L"SpatialAudioProcessor::DeletePluginInstance(): deleted node " << deletedPluginNodeId.uid
                << L", connected prior node " << priorNodeId.uid
                << L" to subsequent plugin node " << subsequentPluginNodeId.uid
                << L" and subsequent drywet node " << subsequentDryWetNodeId.uid;
            Graph()->Log(wstr.str());
        }
    }

    // and clean up state
    {
        _pluginInstances.erase(_pluginInstances.begin() + (pluginInstanceIndex - 1));
        _pluginNodeIds.erase(_pluginNodeIds.begin() + (pluginInstanceIndex - 1));
        _dryWetNodeIds.erase(_dryWetNodeIds.begin() + (pluginInstanceIndex - 1));
    }

    // this is an async update (if we weren't running JUCE in such a hacky way, we wouldn't need to know this)
    Graph()->JuceGraphChanged();
}
