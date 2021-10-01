// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include <algorithm>

#include "Clock.h"
#include "GetBuffer.h"
#include "Histogram.h"
#include "MagicConstants.h"
#include "NowSoundLib.h"
#include "NowSoundGraph.h"
#include "NowSoundTrack.h"
#include "NowSoundInput.h"

using namespace std;
using namespace winrt;

using namespace winrt::Windows::Foundation;

namespace NowSound
{
    NowSoundInputAudioProcessor::NowSoundInputAudioProcessor(
        NowSoundGraph* nowSoundGraph,
        AudioInputId inputId,
        BufferAllocator<float>* audioAllocator,
        int channel)
        : SpatialAudioProcessor(nowSoundGraph, MakeName(L"Input ", (int)inputId), /*initialVolume*/1.0, /*initialPan*/0.5),
        _audioInputId{ inputId },
        _channel{ channel },
        _incomingAudioStream {
            /*initialTime:*/0,
            /*channelCount*/1,
            audioAllocator,
            /*maxBufferedLength*/(int)(Clock::Instance().SampleRateHz() * nowSoundGraph->PreRecordingDuration().Value()) * 2,
            /*useExactLoopingMapper:*/false
        },
        _rawInputHistogram{ new Histogram((int)Clock::Instance().TimeToSamples(MagicConstants::RecentVolumeDuration).Value()) },
        _mutex{}
    {
    }

    NowSoundSpatialParameters NowSoundInputAudioProcessor::SpatialParameters()
    {
        NowSoundSpatialParameters ret;
        ret.Volume = 0; // TODO: fix this by going to output node
        ret.Pan = Pan();
        return ret;
    }

    NowSoundTrackAudioProcessor* NowSoundInputAudioProcessor::CreateRecordingTrack(TrackId id)
    {
        NowSoundTrackAudioProcessor* track = new NowSoundTrackAudioProcessor(Graph(), id, _audioInputId, _incomingAudioStream, Volume(), Pan());

        // Add the new track to the collection of tracks in NowSoundTrackAPI.
        Graph()->AddTrack(id, track);

        return track;
    }

    NowSoundSignalInfo NowSoundInputAudioProcessor::RawSignalInfo()
    {
        std::lock_guard<std::mutex> guard(_mutex);
        float min = _rawInputHistogram->Min();
        float max = _rawInputHistogram->Max();
        float avg = _rawInputHistogram->Average();
        return CreateNowSoundSignalInfo(min, max, avg);
    }

    void NowSoundInputAudioProcessor::processBlock(AudioBuffer<float>& audioBuffer, MidiBuffer& midiBuffer)
    {
        // temporary debugging code: see if processBlock is ever being called under Holofunk
        if (CheckLogThrottle()) {
            std::wstringstream wstr{};
            wstr << getName() << L"::processBlock: count " << NextCounter();
            NowSoundGraph::Instance()->Log(wstr.str());
        }

        // HACK!!!  If this is the zeroth input, then update the audio graph time.
        // We don't really have a great graph-level place to receive notifications from the JUCE graph,
        // so this is really a reasonable spot if you squint hard enough.  (At least it is always
        // connected and always receiving data.)
        if (_audioInputId == AudioInput1)
        {
            Clock::Instance().AdvanceFromAudioGraph(audioBuffer.getNumSamples());
        }

        // Because the input channels get wired up separately to channel 0 of each NowSoundInput, this should always be 0 here
        // even for the input corresponding to channel 1.  (I THINK)
        const float* buffer = audioBuffer.getReadPointer(0);

        // update raw input data because need ALL THE SIGNAL DATA
        for (int i = 0; i < audioBuffer.getNumSamples(); i++)
        {
            _rawInputHistogram->Add(std::abs(buffer[i]));
        }

        _incomingAudioStream.Append(audioBuffer.getNumSamples(), buffer);

        // now process the input audio spatially so we hear it panned in the output
        SpatialAudioProcessor::processBlock(audioBuffer, midiBuffer);
    }
}
