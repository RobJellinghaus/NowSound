// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include <vector>

#include "stdint.h"

#include "NowSoundFrequencyTracker.h"

using namespace RosettaFFT;
using namespace std;

namespace NowSound
{
    NowSoundFrequencyTracker::NowSoundFrequencyTracker(
        const std::vector<FrequencyBinBounds>* bounds,
        int fftSize)
        : _fftBuffer{ std::unique_ptr<Complex>(new Complex[fftSize]) },
        _outputBuffer{ std::unique_ptr<float>(new float[bounds->size()]) },
        _recordingBufferSize{ 0 },
        _binBounds(bounds),
        _fftSize{ fftSize }
    {
        // TODO: BROKEN: std::fill(_outputBuffer.get(), _outputBuffer.get() + bounds->size(), 0);
    }

    void NowSoundFrequencyTracker::GetLatestHistogram(float* outputBuffer, int capacity)
    {
        Check(capacity == _binBounds->size());

        // No thread synchronization here.  Slightly inconsistent data is fine.
        std::copy(_outputBuffer.get(), _outputBuffer.get() + capacity, outputBuffer);
    }

    void NowSoundFrequencyTracker::Record(const float* buffer0, const float* buffer1, int sampleCount)
    {
        Check(_recordingBufferSize <= _fftSize);

        int inputPosition = 0;
        while (sampleCount > 0)
        {
            int recordingBufferCapacity = _fftSize - _recordingBufferSize;

            int samplesToRecord = sampleCount > recordingBufferCapacity ? recordingBufferCapacity : sampleCount;

            Complex* recordingBuffer = _fftBuffer.get();

            for (int i = 0; i < samplesToRecord; i++)
            {
                // Assigning float to complex leaves imaginary value as 0, as desired.
                // Note that std::copy is inapplicable here as we are writing into a Complex array.
                // TODO: add back Blackman-Harris windowing here
                recordingBuffer[_recordingBufferSize + i] = buffer0[inputPosition + i] / 2 + buffer1[inputPosition + 1] / 2;
            }

            _recordingBufferSize += samplesToRecord;
            if (_recordingBufferSize == _fftSize)
            {
                // this buffer is full.
                _recordingBufferSize = 0;
                TransformBuffer();
            }

            sampleCount -= samplesToRecord;
            inputPosition += samplesToRecord;
        }
    }
    
    void NowSoundFrequencyTracker::TransformBuffer()
    {
        // actually run the FFT in placeB!
        RosettaFFT::Complex* fftBuffer = _fftBuffer.get();
        RosettaFFT::CArray fftArray(fftBuffer, _fftSize);

        // and run it!
        RosettaFFT::optimized_fft(fftArray);

        // and rescale it!
        RosettaFFT::RescaleFFT(*_binBounds, fftArray, _outputBuffer.get(), static_cast<int>(_binBounds->size()));
    }
}
