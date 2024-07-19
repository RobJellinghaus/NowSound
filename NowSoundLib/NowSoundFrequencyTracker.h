// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include <complex>
#include <vector>

#include "stdafx.h"

#include "Clock.h"
#include "Histogram.h"
#include "NowSoundLibTypes.h"
#include "rosetta_fft.h"
#include "NowSoundTime.h"

namespace NowSound
{
    // Tracks the frequencies of a stream of input audio, and runs an FFT on that data.
    // Ultimately the tracker allows copying the current histogram of binned values.
    class NowSoundFrequencyTracker
    {
    private:
        // The FFT buffer.
        std::unique_ptr<RosettaFFT::Complex> _fftBuffer;

        // The single lock-free output buffer.
        // This may get written and read concurrently, which is fine; slightly inconsistent data
        // is better than locking overhead.
        std::unique_ptr<float> _outputBuffer;

        // The current size of the recording buffer.
        int _recordingBufferSize;

        // The per-bin bounds; equal in length to the number of bins.
        const std::vector<RosettaFFT::FrequencyBinBounds>* _binBounds;

        // The total FFT size, measured as number of samples in the FFT window.
        const int _fftSize;

    private:
        // Run the FFT inside a task and update the requisite output buffer.
        void TransformBuffer();

    public:
        NowSoundFrequencyTracker(
            const std::vector<RosettaFFT::FrequencyBinBounds>* bounds,
            int fftSize);

        // Get the latest histogram of output values.
        void GetLatestHistogram(float* outputBuffer, int capacity);

        // Record the given amount of float data.
        void Record(const float* channel0, const float* channel1, int sampleCount);
    };
}
