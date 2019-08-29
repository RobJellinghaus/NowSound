// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"

#include "NowSoundFrequencyTracker.h"
#include "NowSoundGraph.h"
#include "BaseAudioProcessor.h"
#include "MeasurableAudio.h"

namespace NowSound
{
    // Measures the contents of its process block, measuring all output channels; also tracks frequencies
    // and supports recording to file.
    // Subclasses can use this to measure either before or after subclass processing.
    class MeasurementAudioProcessor : public BaseAudioProcessor, public MeasurableAudio
    {
        // Mutex to use when returning or updating signal info; to prevent racing between data access and update.
        std::mutex _frequencyDataMutex;

        // histogram of volume
        std::unique_ptr<Histogram> _volumeHistogram;

        // The frequency tracker for the audio traveling through this processor.
        // TODOFX: make this actually track the *post-effects* audio... probably via its own tracker at that stage?
        const std::unique_ptr<NowSoundFrequencyTracker> _frequencyTracker;

        // The file to which we are currently recording.
        File _recordingFile;

        // The thread doing the recording.
        std::unique_ptr<TimeSliceThread> _recordingThread;

        // The ThreadedWriter which is writing to the file.
        std::unique_ptr<AudioFormatWriter::ThreadedWriter> _recordingThreadedWriter;

        // Pointer to the ThreadedWriter, if it exists; this pointer field is the rendezvous between UI thread and audio thread.
        AudioFormatWriter::ThreadedWriter* _recordingThreadedWriterPointer;

        // Mutex for synchronization when starting/stopping recording.
        std::mutex _recordingMutex;

    public:
        MeasurementAudioProcessor(NowSoundGraph* graph, const std::wstring& name);

        // Process the given buffer; use the number of output channels as the channel count.
        // This locks the info mutex.
        virtual void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;

        // Copy out the volume signal info for reading.
        // This locks the info mutex.
        NowSoundSignalInfo SignalInfo();

        // Get the frequency histogram, by updating the given WCHAR buffer as though it were a float* buffer.
        // This locks the info mutex.
        void GetFrequencies(void* floatBuffer, int floatBufferCapacity);

        // Start recording to the given file (WAV format); ignored if already recording.
        void StartRecording(LPWSTR fileName, int32_t fileNameLength);

        // Stop recording; ignored if not recording.
        void StopRecording();
    };
}

#pragma once
