// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include "Check.h"
#include "rosetta_fft.h"

using namespace std;

namespace RosettaFFT
{
    // C++ FFT code from http://rosettacode.org/wiki/Fast_Fourier_transform#C.2B.2B

    // Cooley-Tukey FFT (in-place, divide-and-conquer)
    // Higher memory requirements and redundancy although more intuitive
    void simple_fft(CArray& x)
    {
        const size_t N = x.size();
        if (N <= 1) return;

        // divide
        CArray even = x[std::slice(0, N / 2, 2)];
        CArray  odd = x[std::slice(1, N / 2, 2)];

        // conquer
        simple_fft(even);
        simple_fft(odd);

        // combine
        for (size_t k = 0; k < N / 2; ++k)
        {
            Complex t = std::polar(1.0, -2 * PI * k / N) * odd[k];
            x[k] = even[k] + t;
            x[k + N / 2] = even[k] - t;
        }
    }

    // Cooley-Tukey FFT (in-place, breadth-first, decimation-in-frequency)
    // Better optimized but less intuitive
    // !!! Warning : in some cases this code make result different from not optimased version above (need to fix bug)
    // The bug is now fixed @2017/05/30 
    void optimized_fft(CArray &x)
    {
        // DFT
        unsigned int N = (unsigned int)x.size(), k = N, n;
        double thetaT = PI / N;
        Complex phiT = Complex(cos(thetaT), -sin(thetaT)), T;
        while (k > 1)
        {
            n = k;
            k >>= 1;
            phiT = phiT * phiT;
            T = 1.0L;
            for (unsigned int l = 0; l < k; l++)
            {
                for (unsigned int a = l; a < N; a += n)
                {
                    unsigned int b = a + k;
                    Complex t = x[a] - x[b];
                    x[a] += x[b];
                    x[b] = t * T;
                }
                T *= phiT;
            }
        }
        // Decimate
        unsigned int m = (unsigned int)log2(N);
        for (unsigned int a = 0; a < N; a++)
        {
            unsigned int b = a;
            // Reverse bits
            b = (((b & 0xaaaaaaaa) >> 1) | ((b & 0x55555555) << 1));
            b = (((b & 0xcccccccc) >> 2) | ((b & 0x33333333) << 2));
            b = (((b & 0xf0f0f0f0) >> 4) | ((b & 0x0f0f0f0f) << 4));
            b = (((b & 0xff00ff00) >> 8) | ((b & 0x00ff00ff) << 8));
            b = ((b >> 16) | (b << 16)) >> (32 - m);
            if (b > a)
            {
                swap(x[a], x[b]);
            }
        }
    }

    void CreateBlackmanHarrisWindow(int fftSize, double* data)
    {
        // now let's compute a Blackman-Harris window!
        // https://en.wikipedia.org/wiki/Window_function#Blackman–Harris_window

        double twoPiOverNMinus1 = 2 * PI / (fftSize - 1);
        for (int i = 0; i < fftSize; i++)
        {
            data[i] = 0.42 - (0.5 * std::cos(i * twoPiOverNMinus1)) + (0.08 * std::cos(2 * i * twoPiOverNMinus1));
        }
    }

    void MakeBinBounds(
        std::vector<FrequencyBinBounds>& results,
        double centralFrequency,
        int octaveDivisions,
        size_t binCount,
        int centralBinIndex,
        double sampleRate,
        int fftBinCount)
    {
        NowSound::Check(centralFrequency > 0);
        NowSound::Check(octaveDivisions > 0);
        NowSound::Check(binCount > 0);
        NowSound::Check(centralBinIndex >= 0);
        NowSound::Check(centralBinIndex < binCount);
        NowSound::Check(sampleRate > 0);
        NowSound::Check(fftBinCount > 0);

        vector<double> centralBinFrequencies{};
        centralBinFrequencies.resize(binCount);
        centralBinFrequencies[centralBinIndex] = centralFrequency;
        double binRatio = std::pow(2, (double)1 / octaveDivisions);
        double freq = centralFrequency;
        for (int i = centralBinIndex - 1; i >= 0; i--)
        {
            freq /= binRatio;
            centralBinFrequencies[i] = freq;
        }
        freq = centralFrequency;
        for (int i = centralBinIndex + 1; i < binCount; i++)
        {
            freq *= binRatio;
            centralBinFrequencies[i] = freq;
        }

        // now build up the bounds table
        double bandwidthPerFFTBin = sampleRate / fftBinCount;
        double lowerBound = 0;
        for (int i = 0; i < binCount; i++)
        {
            // we are effectively splitting each bin in half, which leads to an "inter-bin ratio" that is sqrt(binRatio)
            double upperBound = centralBinFrequencies[i] * std::sqrt(binRatio);

            // convert lowerBound and upperBound to source FFT bin indices
            results[i] = FrequencyBinBounds(lowerBound / bandwidthPerFFTBin, upperBound / bandwidthPerFFTBin);
            lowerBound = upperBound;
        }

        // and force final upper bound to be all the way to the middle of the FFT data
        FrequencyBinBounds final = results[results.size() - 1];
        results[results.size() - 1] = FrequencyBinBounds(final.LowerBound, fftBinCount / 2);
    }

    void RescaleFFT(const vector<FrequencyBinBounds>& bounds, const CArray& fftData, float* outputVector, int outputCapacity)
    {
        NowSound::Check(bounds.size() == outputCapacity);

        for (int i = 0; i < bounds.size(); i++)
        {
            // Sum up all fftData slots.
            double count = 0;
            double total = 0;

            // start with fractional part of lower bound
            double lowerBound = bounds[i].LowerBound;
            int lowerBoundFloor = (int)std::floor(lowerBound);
            double lowerBoundFraction = lowerBound - lowerBoundFloor;
            double upperBound = bounds[i].UpperBound;
            int upperBoundFloor = (int)std::floor(upperBound);
            double upperBoundFraction = upperBound - upperBoundFloor;

            if (i > 0)
            {
                double value = abs(fftData[lowerBoundFloor]);

                if (lowerBoundFloor == upperBoundFloor)
                {
                    // this is the only interval that matters
                    count = upperBoundFraction - lowerBoundFraction;
                    total = value * count;

                    // set upperBoundFraction artificially to 0 to cause the final "if" to be skipped
                    upperBoundFraction = 0;
                }
                else
                {
                    count += (1 - lowerBoundFraction);
                    total += ((1 - lowerBoundFraction) * value);
                    lowerBoundFloor++;
                }

                // wcout << fixed << setprecision(5) << L"lowerBound: " << lowerBound << L"; value " << value << L"; count " << count << L"; total " << total << endl;
            }

            // now add in all full buckets up to upperBoundFloor
            for (int j = lowerBoundFloor; j < upperBoundFloor; j++)
            {
                double value = abs(fftData[j]);
                count += 1;
                total += value;

                // wcout << L"adding index " << j << L": " << value << L"; count " << count << L"; total " << total << endl;
            }

            // finally, add in the fractional part of upperBound, if any
            if (upperBoundFraction > 0)
            {
                double value = abs(fftData[upperBoundFloor]);
                count += upperBoundFraction;
                total += value * upperBoundFraction;

                // wcout << L"upperBound: " << upperBound << L"; value " << value << L"; count " << count << L"; total " << total << endl;
            }

            // set the output bin to the average
            outputVector[i] = (float)(total / count);
            // wcout << L"outputVector[" << i << L"] = " << outputVector[i] << endl;
        }
    }
}