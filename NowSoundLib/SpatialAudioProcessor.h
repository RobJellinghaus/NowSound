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
    // stereo panning only), applies a possible internal chain of PluginProgramInstances (instantiated as
    // VSTAudioProcessor nodes), and provides an output MeasurementAudioProcessor for measuring the final audio.
    class SpatialAudioProcessor : public BaseAudioProcessor, public MeasurableAudio
    {
        // current pan value; 0 = left, 0.5 = center, 1 = right
        float _pan;

        // current volume; 0 to 1
        float _volume;

        // is this currently muted?
        // if so, output audio is zeroed
        bool _isMuted;

        // instantiated plugin instances
        std::vector<NowSoundPluginInstanceInfo> _pluginInstances;

        // instantiated plugin node IDs
        std::vector<juce::AudioProcessorGraph::NodeID> _pluginNodeIds;

        // instantiated drywet node IDs, one-to-one with plugin nodes
        std::vector<juce::AudioProcessorGraph::NodeID> _dryWetNodeIds;

        // MeasurementAudioProcessor that carries the output of the effect chain.
        // This is not an owning reference; the JUCE graph owns all processors.
        MeasurementAudioProcessor* _outputProcessor;

    public:
        SpatialAudioProcessor(NowSoundGraph* graph, const std::wstring& name, bool isMuted, float initialVolume, float initialPan);

        // Expect channel 0 to have mono audio data; update all channels with FX-applied output.
        // Will clamp output values in the range (-1.0, 1.0); volumes above 1.0 are not recommended unless the whole loop is quiet
        // enough not to clip.
        virtual void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;

        // Assign both input and output node IDs at once.
        // This allows the processor to do its own internal JUCE graph connections and setup as well.
        void SetNodeIds(juce::AudioProcessorGraph::NodeID inputNodeId, juce::AudioProcessorGraph::NodeID outputNodeId);

        // Get a (non-owning) pointer to the MeasurementAudioProcessor that carries the output of this SpatialAudioProcessor.
        // This provides the signal info and frequencies of the post-sound-effected input audio.
        MeasurementAudioProcessor* OutputProcessor() { return _outputProcessor; }

        // Get the output signal information of this processor (post-effects).
        virtual NowSoundSignalInfo SignalInfo() { return _outputProcessor->SignalInfo(); }

        // Get the output frequency histogram, writing it into this (presumed) vector of floats.
        virtual void GetFrequencies(void* floatBuffer, int floatBufferCapacity) { _outputProcessor->GetFrequencies(floatBuffer, floatBufferCapacity); }
        
        // True if this is muted.
        // 
        // Note that something can be in FinishRecording state but still be muted, if the user is fast!
        // Hence this is a separate flag, not represented as a NowSoundTrack_State.
        bool IsMuted() const;
        void IsMuted(bool isMuted);

        // Get and set the pan value for this track. Values range from 0 (left) to 1 (right).
        float Pan() const;
        void Pan(float pan);

        // Get and set the volume of this track. 0 = mute; 1 = original input level. Use with caution; clipping can occur.
        float Volume() const;
        void Volume(float volume);

        // Delete this processor, by dropping all its nodes.
        void Delete();

        // Install a new instance of a plugin with the specified program and wetdry level.
        // Currently all new plugins go on the end of the chain.
        PluginInstanceIndex AddPluginInstance(PluginId pluginId, ProgramId programId, int dryWet_0_100);

        // Set the dry/wet ratio for the given plugin.
        void SetPluginInstanceDryWet(PluginInstanceIndex pluginInstanceIndex, int dryWet_0_100);

        // Delete a plugin instance. Will cause all subsequent instances to be renumbered.
        // (e.g. plugin instance IDs are really just indexes into the current sequence, not
        // persistent values.)
        void DeletePluginInstance(PluginInstanceIndex pluginInstanceIndex);

        // Get the number of plugin instances on this input.
        int GetPluginInstanceCount();

        // Get info about a plugin instance.
        NowSoundPluginInstanceInfo GetPluginInstanceInfo(PluginInstanceIndex pluginInstanceIndex);

    protected: 
        static std::wstring MakeName(const wchar_t* label, int id)
        {
            std::wstringstream wstr;
            wstr << label << id;
            return wstr.str();
        }
        static std::wstring MakeName(const std::wstring& str1, const std::wstring& str2)
        {
            std::wstringstream wstr;
            wstr << str1 << str2;
            return wstr.str();
        }
    };
}
