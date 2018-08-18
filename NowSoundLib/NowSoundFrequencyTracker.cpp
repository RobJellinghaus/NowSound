// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include <vector>

#include "stdint.h"

#include "NowSoundFrequencyTracker.h"

using namespace concurrency;
using namespace RosettaFFT;
using namespace std;
using namespace std::chrono;
using namespace winrt;
using namespace Windows::Foundation;

namespace NowSound
{
	NowSoundFrequencyTracker::NowSoundFrequencyTracker(
		const std::vector<FrequencyBinBounds>* bounds,
		int fftSize)
		: _bufferStates{},
		_fftBuffers{},
		_outputBuffer{},
		_latestOutputBufferIndex{ -1 },
		_recordingBufferIndex{ 0 },
		_recordingBufferSize{ 0 },
		_binBounds(bounds),
		_fftSize{ fftSize }
	{
		_outputBuffer = std::unique_ptr<float>(new float[bounds->size()]);
		std::fill(_outputBuffer.get(), _outputBuffer.get() + bounds->size(), 0);
		for (int i = 0; i < BufferCount; i++)
		{
			_bufferStates.push_back(BufferState::Available);
			_fftBuffers.push_back(std::unique_ptr<Complex>(new Complex[fftSize]));
		}
		_bufferStates[0] = BufferState::Recording;
	}

	void NowSoundFrequencyTracker::GetLatestHistogram(float* outputBuffer, int capacity)
	{
		Check(capacity == _binBounds->size());

		// No thread synchronization here.  Slightly inconsistent data is fine.
		std::copy(_outputBuffer.get(), _outputBuffer.get() + _binBounds->size(), outputBuffer);
	}

	void NowSoundFrequencyTracker::Record(float* monoInputBuffer, int sampleCount)
	{
		// lock the buffers as we may very well fill the recording buffer now
		std::lock_guard<std::mutex> guard(_bufferMutex);

		Check(_bufferStates[_recordingBufferIndex] == BufferState::Recording);
		Check(_recordingBufferSize < _fftSize);

		int inputPosition = 0;
		while (sampleCount > 0)
		{
			int recordingBufferCapacity = _fftSize - _recordingBufferSize;

			int samplesToRecord = sampleCount > recordingBufferCapacity ? recordingBufferCapacity : sampleCount;

			Complex* recordingBuffer = _fftBuffers[_recordingBufferIndex].get();

			for (int i = 0; i < samplesToRecord; i++)
			{
				// Assigning float to complex leaves imaginary value as 0, as desired.
				// Note that std::copy is inapplicable here as we are writing into a Complex array.
				// TODO: add back Blackman-Harris windowing here
				recordingBuffer[_recordingBufferSize + i] = monoInputBuffer[inputPosition + i];
			}

			_recordingBufferSize += samplesToRecord;
			if (_recordingBufferSize == _fftSize)
			{
				// this buffer is full.
				// If we are still transforming something, then just reset this buffer.
				bool reset = false;
				for (int i = 0; i < BufferCount; i++)
				{
					if (_bufferStates[i] == BufferState::Transforming)
					{
						// just reset this one
						reset = true;
						break;
					}
				}

				if (reset)
				{
					// Throw away the data and start again.  If we are running behind on transformation,
					// it won't help us catch up to continually cancel the overly slow transformations.
					_recordingBufferSize = 0;
				}
				else
				{
					int transformingBufferIndex = _recordingBufferIndex;
					_bufferStates[transformingBufferIndex] = BufferState::Transforming;
					bool found = false;
					for (int i = 0; i < BufferCount; i++)
					{
						if (_bufferStates[i] == BufferState::Available)
						{
							found = true;
							_recordingBufferIndex = i;
							_recordingBufferSize = 0;
							_bufferStates[i] = BufferState::Recording;
							break;
						}
					}

					// We had better have found an available buffer, or we need more buffers or task cancellation or something
					// because we are not managing to keep up.
					Check(found);

					// Spawn task to transform the buffer.
					create_task([this, transformingBufferIndex]() -> void { TransformBufferAsync(transformingBufferIndex); });
				}
			}

			sampleCount -= samplesToRecord;
			inputPosition += samplesToRecord;
		}
	}
	
	void NowSoundFrequencyTracker::TransformBufferAsync(int transformingBufferIndex)
	{
		Check(_bufferStates[transformingBufferIndex] == BufferState::Transforming);

		// actually run the FFT in placeB!
		RosettaFFT::Complex* fftBuffer = _fftBuffers[transformingBufferIndex].get();
		RosettaFFT::CArray fftArray(fftBuffer, _fftSize);

		// and run it!
		RosettaFFT::optimized_fft(fftArray);

		// and rescale it!
		RosettaFFT::RescaleFFT(*_binBounds, fftArray, _outputBuffer.get(), _binBounds->size());

		// and now release our transforming buffer and update output buffer index!
		std::lock_guard<std::mutex> guard(_bufferMutex);
		Check(_bufferStates[transformingBufferIndex] == BufferState::Transforming);
		_bufferStates[transformingBufferIndex] = BufferState::Available;
		_latestOutputBufferIndex = transformingBufferIndex;
	}
}
