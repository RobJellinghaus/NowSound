// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"

#include "NowSoundFrequencyTracker.h"
#include "NowSoundGraph.h"
#include "BaseAudioProcessor.h"

namespace NowSound
{
    // Expects one input channel and N output channels; applies appropriate spatialization (at the moment,
    // stereo panning only) and writes to all N output channels.
    class SpatialAudioProcessor : public BaseAudioProcessor
    {
        // NowSoundGraph we're part of
        NowSoundGraph* _graph;

        // current pan value; 0 = left, 0.5 = center, 1 = right
        float _pan;

        // is this currently muted?
        // if so, output audio is zeroed
        bool _isMuted;

        // histogram of volume
        Histogram _volumeHistogram;

        // The frequency tracker for the audio traveling through this processor.
        // TODOFX: make this actually track the *post-effects* audio... probably via its own tracker at that stage?
        const std::unique_ptr<NowSoundFrequencyTracker> _frequencyTracker;

    public:
        SpatialAudioProcessor(NowSoundGraph* graph, float initialPan);

        // True if this is muted.
        // 
        // Note that something can be in FinishRecording state but still be muted, if the user is fast!
        // Hence this is a separate flag, not represented as a NowSoundTrack_State.
        bool IsMuted() const;
        void IsMuted(bool isMuted);

        // Get and set the pan value for this track.
        float Pan() const;
        void Pan(float pan);
        
        // Get the volume histogram for reading.
        const Histogram& VolumeHistogram() const;

        // Expect channel 0 to have mono audio data; update all channels with FX-applied output.
        virtual void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;

        // The graph this processor is part of.
        NowSoundGraph* Graph() const { return _graph; }

        // Get the frequency histogram, by updating the given WCHAR buffer as though it were a float* buffer.
        void GetFrequencies(void* floatBuffer, int floatBufferCapacity);
    };
}
