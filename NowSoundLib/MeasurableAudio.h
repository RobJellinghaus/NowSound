// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"

#include "NowSoundFrequencyTracker.h"
#include "NowSoundGraph.h"
#include "BaseAudioProcessor.h"

namespace NowSound
{
	// Interface type 
	class MeasurableAudio 
	{
		// Copy out the volume signal info for reading.
		// This locks the info mutex.
		virtual NowSoundSignalInfo SignalInfo() = 0;

		// Get the frequency histogram, by updating the given WCHAR buffer as though it were a float* buffer.
		// This locks the info mutex.
		virtual void GetFrequencies(void* floatBuffer, int floatBufferCapacity) = 0;
	};
}

#pragma once
#pragma once
