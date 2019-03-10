// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include "Clock.h"
#include "MagicConstants.h"
#include "SpatialAudioProcessor.h"

using namespace NowSound;

SpatialAudioProcessor::SpatialAudioProcessor(NowSoundGraph* graph, float initialPan) :
    _isMuted{ false },
    _volumeHistogram{ (int)Clock::Instance().TimeToSamples(MagicConstants::RecentVolumeDuration).Value() },
    _pan{ initialPan },
    _frequencyTracker{ graph->FftSize() < 0
        ? ((NowSoundFrequencyTracker*)nullptr)
        : new NowSoundFrequencyTracker(graph->BinBounds(), graph->FftSize()) }
{}

bool SpatialAudioProcessor::IsMuted() const { return _isMuted; }
void SpatialAudioProcessor::IsMuted(bool isMuted) { _isMuted = isMuted; }

float SpatialAudioProcessor::Pan() const { return _pan; }
void SpatialAudioProcessor::Pan(float pan) { _pan = pan; }

const Histogram& SpatialAudioProcessor::VolumeHistogram() const { return _volumeHistogram; }

void SpatialAudioProcessor::GetFrequencies(void* floatBuffer, int floatBufferCapacity)
{
    if (_frequencyTracker == nullptr)
    {
        return;
    }

    _frequencyTracker->GetLatestHistogram((float*)floatBuffer, floatBufferCapacity);
}

const double Pi = std::atan(1) * 4;

void SpatialAudioProcessor::processBlock(AudioBuffer<float>& audioBuffer, MidiBuffer& midiBuffer)
{
    Check(audioBuffer.getNumChannels() == 2);

    int numSamples = audioBuffer.getNumSamples();

    // Coefficients for panning the mono data into the audio buffer.
    // Use cosine panner for volume preservation.
    double angularPosition = _pan * Pi / 2;
    double leftCoefficient = std::cos(angularPosition);
    double rightCoefficient = std::sin(angularPosition);

    int bufferIndex = 0;
    float* outputBufferChannel0 = audioBuffer.getWritePointer(0);
    float* outputBufferChannel1 = audioBuffer.getWritePointer(1);

    // Pan each mono sample (and track its volume), if we're not muted.
    for (int i = 0; i < numSamples; i++)
    {
        float value = outputBufferChannel0[i];
        _volumeHistogram.Add(std::abs(value));
        outputBufferChannel0[i] = (float)(leftCoefficient * value);
        outputBufferChannel1[i] = (float)(rightCoefficient * value);
    }

    // and provide it to frequency histogram as well
    if (_frequencyTracker != nullptr)
    {
        _frequencyTracker->Record(audioBuffer.getReadPointer(0), numSamples);
    }
}
