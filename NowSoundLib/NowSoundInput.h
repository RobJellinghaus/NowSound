#pragma once

#include "stdafx.h"

#include <future>
#include <vector>

#include <string>
#include "stdint.h"

#include "BaseAudioProcessor.h"
#include "BufferAllocator.h"
#include "Check.h"
#include "Histogram.h"
#include "NowSoundLibTypes.h"
#include "Option.h"
#include "SliceStream.h"
#include "SpatialAudioProcessor.h"

#include "JuceHeader.h"

namespace NowSound
{
    class NowSoundGraph;

    // Container object that manages the current sound effects on a particular input, if any.
    class NowSoundInputAudioProcessor : public SpatialAudioProcessor
    {
    private:
        // The audio input ID of this input.
        const AudioInputId _audioInputId;

        // The channel to select from the input device.
        int _channel;

        // Stream that buffers the last second of input audio, for latency compensation.
        // (Not really clear why latency compensation should be needed for NowSoundApp which shouldn't really
        // have any problematic latency... but this was needed for gesture latency compensation with Kinect,
        // so let's at least experiment with it.)
        BufferedSliceStream<AudioSample, float> _incomingAudioStream;

        // Volume histogram for recording the raw input volume.
        std::unique_ptr<Histogram> _rawInputHistogram;

        // Mutex to use when returning or updating signal info; to prevent racing between data access and update.
        std::mutex _mutex;

    public:
        // Construct a NowSoundInput.
        NowSoundInputAudioProcessor(
            NowSoundGraph* audioGraph,
            AudioInputId audioInputId,
            BufferAllocator<float>* audioAllocator,
            int channel);

        // Process input audio by recording it into the (bounded) incomingAudioStream.
        virtual void processBlock(juce::AudioBuffer<float>& audioBuffer, juce::MidiBuffer& midiBuffer);
        
        // Get information about this input.
        NowSoundSpatialParameters SpatialParameters();

        // Get the raw signal info for the mono source of this input.
        NowSoundSignalInfo RawSignalInfo();

        // Create a recording track monitoring this input.
        // Note that this is not concurrency-safe with respect to other calls to this method.
        // (It is of course concurrency-safe with respect to ongoing audio activity.)
        // Returns a reference to the newly created Node which holds the new TrackAudioProcessor instance.
        NowSoundTrackAudioProcessor* CreateRecordingTrack(TrackId id);
    };
}
