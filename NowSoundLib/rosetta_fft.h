// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include <complex>
#include <iostream>
#include <valarray>
#include <vector>

namespace RosettaFFT
{
	// C++ FFT code from http://rosettacode.org/wiki/Fast_Fourier_transform#C.2B.2B

	const double PI = std::atan(1) * 4;

	typedef std::complex<double> Complex;
	typedef std::valarray<Complex> CArray;

	void simple_fft(CArray& x);
	void optimized_fft(CArray& x);

	// Subsequent methods are not from rosettacode.

	void CreateBlackmanHarrisWindow(int fftSize, double* data);

	// The bounds of a particular output bin returned from the RescaleFFT method.
	struct FrequencyBinBounds
	{
		double LowerBound;
		double UpperBound;
		FrequencyBinBounds() : LowerBound{}, UpperBound{} { }
		FrequencyBinBounds(double l, double u) : LowerBound{ l }, UpperBound{ u } { }
	};

	// Return a vector of [lower, upper] bounds, in terms of indices into an FFT result array.
	// This can be precalculated and reused for rescaling each new FFT.
	void MakeBinBounds(
		// The vector into which to place results (having more difficulty with && references here than expected...).
		std::vector<FrequencyBinBounds>& results,
		// The frequency on which this whole histogram is based.
		double centralFrequency,
		// The number of divisions to make in each octave.
		// Preferably a factor of 12.
		int octaveDivisions,
		// The number of bins in the returned distribution.
		size_t binCount,
		// The index in the returned distribution of the bin centered on centralFrequency.
		int centralBinIndex,
		// The sample rate at which the FFT data was taken.
		double sampleRate,
		// The number of FFT bins in the FFT data.
		int fftBinCount);

	// Given a precalculated vector of FrequencyBinBounds and some FFT data, populate the output
	// vector from the data according to the bounds.
	// The output vector must be the same length as the bounds vector.
	void RescaleFFT(const std::vector<FrequencyBinBounds>& bounds, const CArray& fftData, std::vector<float>& outputVector);
}
