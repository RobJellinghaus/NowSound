// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include "Clock.h"
#include "MagicConstants.h"
#include "SpatialAudioProcessor.h"

using namespace NowSound;
using namespace std;

SpatialAudioProcessor::SpatialAudioProcessor(NowSoundGraph* graph, const wstring& name, float initialPan) 
	: BaseAudioProcessor(graph, name),
    _isMuted{ false },
    _pan{ initialPan },
	_outputProcessor{ new MeasurementAudioProcessor(graph, name) },
	_pluginInstances{},
	_pluginNodeIds{}
{}

bool SpatialAudioProcessor::IsMuted() const { return _isMuted; }
void SpatialAudioProcessor::IsMuted(bool isMuted) { _isMuted = isMuted; }

float SpatialAudioProcessor::Pan() const { return _pan; }
void SpatialAudioProcessor::Pan(float pan) { _pan = pan; }

const double Pi = std::atan(1) * 4;

void SpatialAudioProcessor::processBlock(AudioBuffer<float>& audioBuffer, MidiBuffer& midiBuffer)
{
    Check(audioBuffer.getNumChannels() == 2);

    int numSamples = audioBuffer.getNumSamples();

    // Coefficients for panning the mono data into the audio buffer.
    // Use cosine panner for volume preservation.
    double angularPosition = _pan * Pi / 2;
    double leftCoefficient = std::cos(angularPosition);
    double rightCoefficient = std::sin(angularPosition);

    float* outputBufferChannel0 = audioBuffer.getWritePointer(0);
    float* outputBufferChannel1 = audioBuffer.getWritePointer(1);

    // Pan each mono sample, if we're not muted.
    for (int i = 0; i < numSamples; i++)
    {
        float value = _isMuted ? 0 : outputBufferChannel0[i];
        outputBufferChannel0[i] = (float)(leftCoefficient * value);
        outputBufferChannel1[i] = (float)(rightCoefficient * value);
    }
}

void SpatialAudioProcessor::SetNodeIds(juce::AudioProcessorGraph::NodeID inputNodeId, juce::AudioProcessorGraph::NodeID outputNodeId)
{
	SetNodeId(inputNodeId);
	OutputProcessor()->SetNodeId(outputNodeId);

	// now set up the connections here
	// TODO: add in effects when pre-creating them
	Check(Graph()->JuceGraph().addConnection({ { inputNodeId, 0 }, { outputNodeId, 0 } }));
	Check(Graph()->JuceGraph().addConnection({ { inputNodeId, 1 }, { outputNodeId, 1 } }));
}

PluginInstanceIndex SpatialAudioProcessor::AddPluginInstance(PluginId pluginId, ProgramId programId, int dryWet_0_100)
{
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

	// remove connections from sourceNode to outputNode
	Check(Graph()->JuceGraph().removeConnection({ { inputNodeId, 0 }, { outputNodeId, 0 } }));
	Check(Graph()->JuceGraph().removeConnection({ { inputNodeId, 1 }, { outputNodeId, 1 } }));

	// add connections from input to new plugin instance...
	Check(Graph()->JuceGraph().addConnection({ { inputNodeId, 0 }, { newNode->nodeID, 0 } }));
	Check(Graph()->JuceGraph().addConnection({ { inputNodeId, 1 }, { newNode->nodeID, 1 } }));

	// ...and from new plugin instance to output
	Check(Graph()->JuceGraph().addConnection({ { newNode->nodeID, 0 }, { outputNodeId, 0 } }));
	Check(Graph()->JuceGraph().addConnection({ { newNode->nodeID, 1 }, { outputNodeId, 1 } }));

	_pluginInstances.push_back(PluginInstanceState(pluginId, programId, dryWet_0_100));
	_pluginNodeIds.push_back(newNode->nodeID);
	return (PluginInstanceIndex)_pluginNodeIds.size();
}

void SpatialAudioProcessor::SetPluginInstanceDryWet(PluginInstanceIndex pluginInstanceIndex, int32_t dryWet_0_100)
{

}

void SpatialAudioProcessor::DeletePluginInstance(PluginInstanceIndex pluginInstanceIndex)
{
	Check(pluginInstanceIndex >= 1);
	Check(pluginInstanceIndex < _pluginNodeIds.size() + 1);

	AudioProcessorGraph::NodeID priorNodeId =
		pluginInstanceIndex == 1 
		? NodeId()
		: _pluginNodeIds[pluginInstanceIndex - 2];

	AudioProcessorGraph::NodeID deletedNodeId = _pluginNodeIds[pluginInstanceIndex - 1];

	AudioProcessorGraph::NodeID subsequentNodeId =
		pluginInstanceIndex == _pluginNodeIds.size() + 1
		? OutputProcessor()->NodeId()
		: _pluginNodeIds[pluginInstanceIndex];
	
	// drop the node and all connections
	Graph()->JuceGraph().removeNode(Graph()->JuceGraph().getNodeForId(deletedNodeId));

	// reconnect prior node to subsequent
	Check(Graph()->JuceGraph().addConnection({ { priorNodeId, 0 }, { subsequentNodeId, 0 } }));
	Check(Graph()->JuceGraph().addConnection({ { priorNodeId, 1 }, { subsequentNodeId, 1 } }));
}
