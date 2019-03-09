// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include <complex>
#include <string>
#include <vector>

#include "stdafx.h"

#include "Clock.h"
#include "Histogram.h"
#include "NowSoundLibTypes.h"
#include "Recorder.h"
#include "rosetta_fft.h"
#include "NowSoundTime.h"

namespace NowSound
{
	// Tracks the frequencies of a stream of input audio, by buffering the audio until a FFT
	// window is accumulated, then firing a task to run an FFT on that data while buffering
	// more.  Ultimately the tracker allows copying the current histogram of binned values.
	// All buffer management and concurrency is internal to this class.
	class NowSoundFrequencyTracker
	{
	private:
		// The number of buffers to create.
		// The expectation is that we will never need more than two, due to transforming being
		// able to keep ahead of recording.
		const int BufferCount = 2;

		// States of a buffer.
		enum BufferState
		{
			// Vacant value
			Uninitialized,
			// Buffer is available for use
			Available,
			// Buffer is being recorded into
			Recording,
			// Buffer is having the FFT executed on it
			Transforming
		};

		// The states of each buffer.
		std::vector<BufferState> _bufferStates;

		// The FFT buffers themselves.
		std::vector<std::unique_ptr<RosettaFFT::Complex>> _fftBuffers;

		// The single lock-free output buffer.
		// This may get written and read concurrently, which is fine; slightly inconsistent data
		// is better than locking overhead.
		std::unique_ptr<float> _outputBuffer;

		// The currently recording buffer index.
		int _recordingBufferIndex;

		// The current size of the recording buffer.
		int _recordingBufferSize;

		// The most recently updated output buffer.
		int _latestOutputBufferIndex;

		// Mutex for buffer changes.
		std::mutex _bufferMutex;

		// The bin bounds.
		const std::vector<RosettaFFT::FrequencyBinBounds>* _binBounds;

		// The FFT size.
		const int _fftSize;

	private:
		// Run the FFT inside a task and update the requisite output buffer.
		void TransformBufferAsync(int transformBufferIndex);

	public:
		NowSoundFrequencyTracker(
			const std::vector<RosettaFFT::FrequencyBinBounds>* bounds,
			int fftSize);

		// Get the latest histogram of output values.
		void GetLatestHistogram(float* outputBuffer, int capacity);

		// Record the given amount of float data.
		void Record(const float* monoInputBuffer, int sampleCount);
	};
}
