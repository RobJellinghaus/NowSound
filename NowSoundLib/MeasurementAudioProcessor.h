// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"

#include "NowSoundFrequencyTracker.h"
#include "NowSoundGraph.h"
#include "BaseAudioProcessor.h"

namespace NowSound
{
    // Measures the contents of its process block, measuring all output channels.
    // Subclasses can use this to measure either before or after subclass processing.
    class MeasurementAudioProcessor : public BaseAudioProcessor
    {
        // NowSoundGraph we're part of
        NowSoundGraph* _graph;

        // Mutex to use when returning or updating signal info; to prevent racing between data access and update.
        std::mutex _mutex;

        // histogram of volume
        std::unique_ptr<Histogram> _volumeHistogram;

        // The frequency tracker for the audio traveling through this processor.
        // TODOFX: make this actually track the *post-effects* audio... probably via its own tracker at that stage?
        const std::unique_ptr<NowSoundFrequencyTracker> _frequencyTracker;

    public:
        MeasurementAudioProcessor(NowSoundGraph* graph);

        virtual const String getName() const override { return L"MeasurementAudioProcessor"; }

        // Process the given buffer; use the number of output channels as the channel count.
        // This locks the info mutex.
        virtual void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;

        // The graph this processor is part of.
        NowSoundGraph* Graph() const { return _graph; }

        // Copy out the volume signal info for reading.
        // This locks the info mutex.
        NowSoundSignalInfo SignalInfo();

        // Get the frequency histogram, by updating the given WCHAR buffer as though it were a float* buffer.
        // This locks the info mutex.
        void GetFrequencies(void* floatBuffer, int floatBufferCapacity);
    };
}

#pragma once
