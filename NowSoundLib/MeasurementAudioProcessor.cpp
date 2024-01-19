// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

using namespace std;

#include "Clock.h"
#include "MagicConstants.h"
#include "MeasurementAudioProcessor.h"

using namespace NowSound;

MeasurementAudioProcessor::MeasurementAudioProcessor(NowSoundGraph* graph, const wstring& name)
    : BaseAudioProcessor(graph, name),
    _frequencyDataMutex{},
    // hardcoded to the clock's channel count, e.g. the overall output bus width.
    _volumeHistogram{ new Histogram((int)(graph->Clock()->TimeToRoundedUpSamples(MagicConstants::RecentVolumeDuration).Value())) },
    _frequencyTracker{ graph->FftSize() < 0
        ? ((NowSoundFrequencyTracker*)nullptr)
        : new NowSoundFrequencyTracker(graph->BinBounds(), graph->FftSize()) },
    _recordingFile{},
    _recordingMutex{},
    _recordingThread{},
    _recordingThreadedWriter{},
    _recordingThreadedWriterPointer{}
{}

NowSoundSignalInfo MeasurementAudioProcessor::SignalInfo()
{
    std::lock_guard<std::mutex> guard(_frequencyDataMutex);
    float min = _volumeHistogram->Min();
    float max = _volumeHistogram->Max();
    float avg = _volumeHistogram->Average();
    return CreateNowSoundSignalInfo(min, max, avg);
}

void MeasurementAudioProcessor::GetFrequencies(void* floatBuffer, int floatBufferCapacity)
{
    // avoid race condition at init time
    if (_frequencyTracker == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(_frequencyDataMutex);
    _frequencyTracker->GetLatestHistogram((float*)floatBuffer, floatBufferCapacity);
}

const double Pi = std::atan(1) * 4;

void MeasurementAudioProcessor::processBlock(AudioBuffer<float>& audioBuffer, MidiBuffer& midiBuffer)
{
    // temporary debugging code: see if processBlock is ever being called under Holofunk
    if (CheckLogThrottle())
    {
        std::wstringstream wstr{};
        wstr << getName() << L"::processBlock: count " << NextCounter();
        NowSoundGraph::Instance()->Log(wstr.str());
    }

    Check(audioBuffer.getNumChannels() == 2);

    int numSamples = audioBuffer.getNumSamples();

    const float* outputBufferChannel0 = audioBuffer.getReadPointer(0);
    const float* outputBufferChannel1 = audioBuffer.getReadPointer(1);

    // Pan each mono sample (and track its volume), if we're not muted.
    {
        std::lock_guard<std::mutex> guard(_frequencyDataMutex);

        for (int i = 0; i < numSamples; i++)
        {
            // we average the stereo channels when computing the histogram values to add
            float value0 = outputBufferChannel0[i];
            float value1 = outputBufferChannel1[i];
            _volumeHistogram->Add(std::abs(value0) / 2 + std::abs(value1) / 2);
        }

        // and provide it to frequency histogram as well
        if (_frequencyTracker != nullptr)
        {
            _frequencyTracker->Record(outputBufferChannel0, outputBufferChannel1, numSamples);
        }
    }

    // and write to recording thread, if any
    {
        std::lock_guard<std::mutex> guard(_recordingMutex);
        if (_recordingThreadedWriterPointer != nullptr)
        {
            _recordingThreadedWriterPointer->write(audioBuffer.getArrayOfReadPointers(), numSamples);
        }
    }
}

void MeasurementAudioProcessor::StartRecording(LPWSTR fileName, int32_t fileNameLength)
{
    // Thanks to the JUCE AudioRecorderDemo example:
    // https://github.com/WeAreROLI/JUCE/blob/master/examples/Audio/AudioRecordingDemo.h

    // we don't need to be under a lock here, because the only way _recordingThreadedWriter changes state
    // is via calling either this method or StopRecording() on the (single) UI thread.  In other words,
    // we deliberately do not make this method concurrency-safe against other calls to either this or
    // StopRecording().  The only concurrency safety here is with respect to updating the value of
    // _recordingThreadedWriter itself, as such updates race with the audio thread's accesses to the writer.
    if (_recordingThreadedWriter.get() != nullptr)
    {
        // already recording; ignore this call
        return;
    }

    _recordingFile = File{ String{ fileName } };

    // TODO: test if this actually works... can we reuse threads across multiple ThreadedWriters?
    if (_recordingThread.get() == nullptr)
    {
        _recordingThread.reset(new TimeSliceThread(L"MeasurementAudioProcessor::_recordingThread"));
        _recordingThread->startThread();
    }

    if (auto fileStream = std::unique_ptr<FileOutputStream>(_recordingFile.createOutputStream()))
    {
        // Now create a WAV writer object that writes to our output stream...
        WavAudioFormat wavFormat;

        if (auto writer = wavFormat.createWriterFor(fileStream.get(), Graph()->Info().SampleRateHz, 2, 32, {}, 0))
        {
            fileStream.release(); // (passes responsibility for deleting the stream to the writer object that is now using it)

            // Now we'll create one of these helper objects which will act as a FIFO buffer, and will
            // write the data to disk on our background thread.
            _recordingThreadedWriter.reset(new AudioFormatWriter::ThreadedWriter(writer, *_recordingThread, 32768));
            _recordingThreadedWriter->setFlushInterval(this->Graph()->Clock()->SampleRateHz() / 4); // only lose 1/4 second at most between flushes

            // time to lock to prevent racing against the audio thread
            std::lock_guard<std::mutex> recordingLock(_recordingMutex);
            // and update the rendezvous pointer field
            _recordingThreadedWriterPointer = _recordingThreadedWriter.get();
        }
    }
}

void MeasurementAudioProcessor::StopRecording()
{
    if (_recordingThreadedWriter.get() == nullptr)
    {
        // not recording; ignore
        return;
    }

    // First, clear this pointer to stop the audio callback from using our writer object.
    // Take the lock only as long as needed to drop the pointer and stop writes instantly.
    {
        std::lock_guard<std::mutex> guard(_recordingMutex);
        _recordingThreadedWriterPointer = nullptr;
    }

    // Now we can delete the writer object. It's done in this order because the deletion could
    // take a little time while remaining data gets flushed to disk, so it's best to avoid blocking
    // the audio callback while this happens.
    _recordingThreadedWriter.reset();

    // Not sure whether to do the same for the thread or not... let's say not, for the time being.
}
