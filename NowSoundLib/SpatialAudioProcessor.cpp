// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include "Clock.h"
#include "MagicConstants.h"
#include "SpatialAudioProcessor.h"

using namespace NowSound;
using namespace std;

SpatialAudioProcessor::SpatialAudioProcessor(NowSoundGraph* graph, const wstring& name, float initialPan) :
    MeasurementAudioProcessor(graph, name),
    _isMuted{ false },
    _pan{ initialPan }
{}

bool SpatialAudioProcessor::IsMuted() const { return _isMuted; }
void SpatialAudioProcessor::IsMuted(bool isMuted) { _isMuted = isMuted; }

float SpatialAudioProcessor::Pan() const { return _pan; }
void SpatialAudioProcessor::Pan(float pan) { _pan = pan; }

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

    // Measure the audio now -- at the moment we only track channel 0 anyway
    MeasurementAudioProcessor::processBlock(audioBuffer, midiBuffer);

    float* outputBufferChannel0 = audioBuffer.getWritePointer(0);
    float* outputBufferChannel1 = audioBuffer.getWritePointer(1);

    // Pan each mono sample (and track its volume), if we're not muted.
    for (int i = 0; i < numSamples; i++)
    {
        float value = outputBufferChannel0[i];
        outputBufferChannel0[i] = (float)(leftCoefficient * value);
        outputBufferChannel1[i] = (float)(rightCoefficient * value);
    }
}
