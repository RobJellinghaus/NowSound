// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

class NowSoundAppMagicNumbers
{
public:
	// Values for the FFT and frequency histogram.

	// 2048 is enough to resolve down to about two octaves below middle C (e.g. 65 Hz).
	static const int FftBinSize = 2048;
	// Number of output bins; this can be whatever we want to see, rendering-wise.
	static const int OutputBinCount = 20;
	// Number of divisions per octave (e.g. setting this to 3 equals four semitones per bin, 12 divided by 3).
	static const int OctaveDivisions = 4;
	// The central frequency of the histogram; this is middle C.
	static const double CentralFrequency;
	// The bin (out of OutputBinCount) in which the central frequency should be mapped; zero-indexed.
	static const int CentralFrequencyBin = 9;
};
