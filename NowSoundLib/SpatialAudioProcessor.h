// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"

#include <string>
#include "NowSoundFrequencyTracker.h"
#include "NowSoundGraph.h"
#include "MeasurementAudioProcessor.h"
#include "MeasurableAudio.h"

namespace NowSound
{
    // Expects one input channel and N output channels; applies appropriate spatialization (at the moment,
    // stereo panning only) and writes to all N output channels.
    // Also supports a chain of PluginProgramInstances.
    class SpatialAudioProcessor : public BaseAudioProcessor, public MeasurableAudio
    {
        // current pan value; 0 = left, 0.5 = center, 1 = right
        float _pan;

        // is this currently muted?
        // if so, output audio is zeroed
        bool _isMuted;

        // vector of PluginInstanceStates
        std::vector<PluginInstanceState>  _pluginInstances;

        // parallel vector of plugin nodeIds
        std::vector<juce::AudioProcessorGraph::NodeID> _pluginNodeIds;

        // MeasurementAudioProcessor that carries the output of the effect chain.
        // This is not an owning reference; the JUCE graph owns all processors.
        MeasurementAudioProcessor* _outputProcessor;

    public:
        SpatialAudioProcessor(NowSoundGraph* graph, const std::wstring& name, float initialPan);

        // Expect channel 0 to have mono audio data; update all channels with FX-applied output.
        virtual void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;

        // Assign both input and output node IDs at once.
        // This allows the processor to do its own internal JUCE graph connections and setup as well.
        void SetNodeIds(juce::AudioProcessorGraph::NodeID inputNodeId, juce::AudioProcessorGraph::NodeID outputNodeId);

        // Get a (non-owning) pointer to the MeasurementAudioProcessor that carries the output of this SpatialAudioProcessor.
        // The intent is to facilitate getting the signal info and frequencies of the post-sound-effected input audio.
        MeasurementAudioProcessor* OutputProcessor() { return _outputProcessor; }

        // Get the output signal information of this processor (post-effects).
        NowSoundSignalInfo SignalInfo() { return _outputProcessor->SignalInfo(); }

        // Get the output frequency histogram, writing it into this (presumed) vector of floats.
        void GetFrequencies(void* floatBuffer, int floatBufferCapacity) { _outputProcessor->GetFrequencies(floatBuffer, floatBufferCapacity); }
        
        // True if this is muted.
        // 
        // Note that something can be in FinishRecording state but still be muted, if the user is fast!
        // Hence this is a separate flag, not represented as a NowSoundTrack_State.
        bool IsMuted() const;
        void IsMuted(bool isMuted);

        // Get and set the pan value for this track.
        float Pan() const;
        void Pan(float pan);

        // Install a new instance of a plugin with the specified program and wetdry level.
        // Currently all new plugins go on the end of the chain.
        PluginInstanceIndex AddPluginInstance(PluginId pluginId, ProgramId programId, int dryWet_0_100);

        // Set the dry/wet ratio for the given plugin.
        void SetPluginInstanceDryWet(PluginInstanceIndex pluginInstanceIndex, int dryWet_0_100);

        // Delete a plugin instance. Will cause all subsequent instances to be renumbered.
        // (e.g. plugin instance IDs are really just indexes into the current sequence, not
        // persistent values.)
        void DeletePluginInstance(PluginInstanceIndex pluginInstanceIndex);

    protected: 
        static std::wstring MakeName(const wchar_t* label, int id)
        {
            std::wstringstream wstr;
            wstr << label << id;
            return wstr.str();
        }
    };
}
