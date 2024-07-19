// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include <sstream>

#include "stdint.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "Clock.h"
#include "GetBuffer.h"
#include "MagicConstants.h"
#include "NowSoundGraph.h"
#include "NowSoundInput.h"
#include "NowSoundLib.h"
#include "NowSoundTrack.h"
#include "Slice.h"
#include "SliceStream.h"
#include "NowSoundTime.h"

using namespace concurrency;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace winrt::Windows::Foundation;

namespace NowSound
{
    NowSoundTrackAudioProcessor::NowSoundTrackAudioProcessor(
        NowSoundGraph* graph,
        TrackId trackId,
        AudioInputId inputId,
        const BufferedSliceStream<AudioSample, float>& sourceStream,
        float initialVolume,
        float initialPan,
        float beatsPerMinute,
        int beatsPerMeasure)
        : SpatialAudioProcessor(graph, MakeName(L"Track ", (int)trackId), /*isMuted:*/false, initialVolume, initialPan),
        _trackId{ trackId },
        _audioInputId{ inputId },
        _state{ NowSoundTrackState::TrackRecording },
        // latency compensation effectively means the track started before it was constructed ;-)
        _audioStream(new BufferedSliceStream<AudioSample, float>(
            1,
            NowSoundGraph::Instance()->AudioAllocator(),
            /*maxBufferedDuration:*/ 0)),
        // one beat is the shortest any track ever is (TODO: allow optionally relaxing quantization)
        _beatDuration{ 1 },
        _priorBeatDuration{ 1 },
        _localLoopTime{ 0 },
        _tempo{ new Tempo(beatsPerMinute, beatsPerMeasure, graph->Clock()->SampleRateHz()) }
    {
        // Tracks should only be created from the UI thread (or at least not from the audio thread).
        // TODO: thread contracts.

        // should only ever call this when graph is fully up and running
        Check(NowSoundGraph::Instance()->State() == NowSoundGraphState::GraphRunning);

        ContinuousDuration<Second> preRecordingDuration = NowSoundGraph::Instance()->PreRecordingDuration();
        if (preRecordingDuration.Value() > 0)
        {
            // Prepend preRecordingDuration seconds of previously buffered input audio, to prepopulate this track.
            Duration<AudioSample> preRecordingSampleDuration = graph->Clock()->TimeToRoundedUpSamples(preRecordingDuration);
            Interval<AudioSample> lastIntervalOfSourceStream(
                Time<AudioSample>(0) + sourceStream.DiscreteDuration() - preRecordingSampleDuration,
                preRecordingSampleDuration,
                Direction::Forwards);
            sourceStream.AppendTo(lastIntervalOfSourceStream, _audioStream.get());
        }

        {
            std::wstringstream wstr{};
            wstr << L"NowSoundTrack::NowSoundTrack(" << trackId << L")";
            NowSoundGraph::Instance()->Log(wstr.str());
        }
    }

    NowSoundTrackAudioProcessor::NowSoundTrackAudioProcessor(TrackId trackId, NowSoundTrackAudioProcessor* other)
        : SpatialAudioProcessor(other->Graph(), MakeName(L"Track ", (int)trackId), other->IsMuted(), other->Volume(), other->Pan()),
        _trackId{ trackId },
        _audioInputId{ other->_audioInputId },
        _state{ NowSoundTrackState::TrackLooping },
        // latency compensation effectively means the track started before it was constructed ;-)
        _audioStream(other->_audioStream),
        // one beat is the shortest any track ever is (TODO: allow optionally relaxing quantization)
        _beatDuration{ other->_beatDuration },
        _priorBeatDuration{ other->_priorBeatDuration },
        _localLoopTime{ other->_localLoopTime },
        _justStoppedRecording{ false },
        _direction{ other->_direction },
        _tempo{ new Tempo(other->_tempo->BeatsPerMinute(), other->_tempo->BeatsPerMeasure(), other->Graph()->Clock()->SampleRateHz()) }
    {
        // we're a copied loop; spam like crazy
        std::wstringstream wstr{};
        wstr << L"NowSoundTrackAudioProcessor copy ctor: other->Info().Volume " << other->Volume() << ", other->Pan() " << other->Pan();
        NowSoundGraph::Instance()->Log(wstr.str());
    }
        
    bool NowSoundTrackAudioProcessor::JustStoppedRecording()
    {
        if (_justStoppedRecording)
        {
            _justStoppedRecording = false;
            return true;
        }
        else
        { 
            return false;
        }
    }

    // If we are recording, monitor the input; otherwise, monitor the track itself.
    NowSoundSignalInfo NowSoundTrackAudioProcessor::SignalInfo()
    {
        if (_state == NowSoundTrackState::TrackRecording
            || _state == NowSoundTrackState::TrackFinishRecording)
        {
            return NowSoundGraph::Instance()->Input(_audioInputId)->SignalInfo();
        }
        else
        {
            return SpatialAudioProcessor::SignalInfo();
        }
    }

    // If we are recording, monitor the input; otherwise, monitor the track itself.
    void NowSoundTrackAudioProcessor::GetFrequencies(void* floatBuffer, int floatBufferCapacity)
    {
        if (_state == NowSoundTrackState::TrackRecording
            || _state == NowSoundTrackState::TrackFinishRecording)
        {
            NowSoundGraph::Instance()->Input(_audioInputId)->GetFrequencies(floatBuffer, floatBufferCapacity);
        }
        else
        {
            return SpatialAudioProcessor::GetFrequencies(floatBuffer, floatBufferCapacity);
        }
    }

    NowSoundTrackState NowSoundTrackAudioProcessor::State() const { return _state; }
    
    Duration<Beat> NowSoundTrackAudioProcessor::BeatDuration() const { return _beatDuration; }

    float NowSoundTrackAudioProcessor::BeatsPerMinute() const { return _tempo->BeatsPerMinute(); }

    int NowSoundTrackAudioProcessor::BeatsPerMeasure() const { return _tempo->BeatsPerMeasure(); }

    ContinuousDuration<Beat> NowSoundTrackAudioProcessor::BeatPositionUnityNow() const
    {
        ContinuousDuration<Beat> beats = _tempo->TimeToBeats(_localLoopTime);
        int completeBeatsSinceStart = (int)beats.Value() % (int)BeatDuration().Value();
        return (ContinuousDuration<Beat>)(completeBeatsSinceStart + (beats.Value() - (int)beats.Value()));
    }

    ContinuousDuration<AudioSample> NowSoundTrackAudioProcessor::ExactDuration() const
    {
        return (int)BeatDuration().Value() * _tempo->BeatDuration().Value();
    }

    NowSoundTrackInfo NowSoundTrackAudioProcessor::Info() 
    {
        Time<AudioSample> lastSampleTime = this->_localLoopTime.RoundedDown(); // to prevent any drift from this being updated concurrently

        bool isLooping = _state == NowSoundTrackState::TrackLooping;

        // While looping, the current track time is _localLoopTime.
        // If not looping, we are recording, and the current track time is how much we've recorded.
        ContinuousTime<AudioSample> localLoopTime =
            isLooping
            ? this->_localLoopTime
            : ContinuousTime<AudioSample>(this->_audioStream.get()->DiscreteDuration().AsContinuous().Value());

        return CreateNowSoundTrackInfo(
            isLooping,
            _direction == Direction::Backwards,
            this->BeatDuration().Value(),
            this->ExactDuration().Value(),
            localLoopTime.Value(),
            _tempo->TimeToBeats(localLoopTime).Value(),
            Pan(),
            Volume(),
            BeatsPerMinute(),
            BeatsPerMeasure());
    }

    void NowSoundTrackAudioProcessor::FinishRecording()
    {
        // TODO: ThreadContract.RequireUnity();

        // no need for any synchronization at all; the Record() logic will see this change.
        // We have no memory fence here but this write does reliably get seen sufficiently quickly in practice.
        _state = NowSoundTrackState::TrackFinishRecording;
    }

    Direction NowSoundTrackAudioProcessor::PlaybackDirection() const
    {
        return _direction;
    }

    void NowSoundTrackAudioProcessor::SetPlaybackDirection(bool isPlaybackBackwards)
    {
        // just poke it in there, it's thread-safe to do so
        _direction = isPlaybackBackwards ? Direction::Backwards : Direction::Forwards;
    }

    void NowSoundTrackAudioProcessor::Rewind()
    {
        // just poke it in there, it's thread-safe to do so
        _localLoopTime = 0;
    }

    const int maxCounter = 1000;

    void NowSoundTrackAudioProcessor::processBlock(AudioBuffer<float>& audioBuffer, MidiBuffer& midiBuffer)
    {
        // temporary debugging code: see if processBlock is ever being called under Holofunk
        /*
        if (CheckLogThrottle()) {
            std::wstringstream wstr{};
            wstr << getName() << L"::processBlock: count " << NextCounter() << L", state " << _state;
            NowSoundGraph::Instance()->Log(wstr.str());
        }
        */
        
        // This should always take two channels.  Only channel 0 is used on input.  Both channels are used
        // on output (only stereo supported for now).
        Check(audioBuffer.getNumChannels() == 2);

        // Depending on the current state of this track, we either record, or we finish recording
        // and switch modes to looping, or we're straight looping.

        // The number of samples in the outgoing buffer.
        Duration<AudioSample> bufferDuration{ audioBuffer.getNumSamples() };

        // The duration that's been copied to the input channel so far (used when looping).
        Duration<AudioSample> completedDuration{ 0 };

        switch (_state)
        {
            case NowSoundTrackState::TrackRecording:
            {
                HandleTrackRecording(bufferDuration, audioBuffer);
                break;
            }

            case NowSoundTrackState::TrackFinishRecording:
            {
                HandleTrackFinishRecording(bufferDuration, audioBuffer);
                break;
            }

            case NowSoundTrackState::TrackLooping:
            {
                HandleTrackLooping(bufferDuration, audioBuffer, completedDuration, midiBuffer);
                break;
            }
        }
    }

    void NowSoundTrackAudioProcessor::HandleTrackRecording(NowSound::Duration<NowSound::AudioSample>& bufferDuration, juce::AudioSampleBuffer& audioBuffer)
    {
        // We are recording; save the input audio in our stream, and that's it (we never call processBlock here,
        // because the input audio is already getting mixed through to the output).

        // How many complete beats after we record this data?
        Time<AudioSample> durationAsTime((_audioStream.get()->DiscreteDuration() + bufferDuration).Value());
        Duration<Beat> completeBeats = (Duration<Beat>)((int)_tempo->TimeToBeats(durationAsTime.AsContinuous()).Value());

        // If it's more than our _beatDuration, bump our _beatDuration
        // TODO: implement other quantization policies here
        if (completeBeats >= _beatDuration)
        {
            _priorBeatDuration = _beatDuration;

            // 1/2/4* quantization, like old times. TODO: make this selectable
            if (_beatDuration == 1)
            {
                _beatDuration = 2;
            }
            else if (_beatDuration == 2)
            {
                _beatDuration = _tempo->BeatsPerMeasure();
            }
            else
            {
                _beatDuration = _beatDuration + Duration<Beat>(_tempo->BeatsPerMeasure());
            }
            // blow up if we happen somehow to be recording more than one beat's worth (should never happen given low latency expectation)
            Check(completeBeats < BeatDuration());
        }

        // and actually record the full amount of available data.
        // Getting data for channel 0 is always correct because the JUCE per-channel connections handle which
        // input channel goes to which track.
        _audioStream.get()->Append(bufferDuration, audioBuffer.getReadPointer(0));

        // and quiet all output channels
        for (int i = 0; i < this->getTotalNumOutputChannels(); i++)
        {
            zeromem(audioBuffer.getWritePointer(i), sizeof(float) * bufferDuration.Value());
        }
    }

    void NowSoundTrackAudioProcessor::HandleTrackFinishRecording(NowSound::Duration<NowSound::AudioSample>& bufferDuration, juce::AudioSampleBuffer& audioBuffer)
    {
        // Finish up and close the audio stream with the last precise samples; again, don't call processBlock.
        ContinuousDuration<AudioSample> lastPriorDuration = _tempo->BeatsToSamples(_priorBeatDuration.AsContinuous());
        if (_audioStream->DiscreteDuration() > lastPriorDuration.RoundedUp()) {
            // Check whether we are within a certain beat duration (the "truncation beats" duration)
            // from the prior track duration.
            // If this is the case, then we retroactively shorten the loop by reverting to the previous
            // track duration. This is by popular demand; almost all users find it confusing to let go
            // of the record button before they are actually done, which almost always means they let go
            // late in realtime.
            ContinuousDuration<Beat> truncationBeats = _priorBeatDuration == 1
                ? MagicConstants::SingleTruncationBeats
                : MagicConstants::MultiTruncationBeats;

            // Add the beats together and THEN round up. The beat number will always be an integer,
            // but the total beat sample count may not be, and should be rounded up only once.
            Duration<AudioSample> truncationDuration =
                _tempo->BeatsToSamples(_priorBeatDuration.AsContinuous() + truncationBeats).RoundedUp();

            if (_audioStream->DiscreteDuration() < truncationDuration) {
                // OK fine, we assume user intended to let go already, so we retroactively return to the prior duration.
                _beatDuration = _priorBeatDuration;

                // What will our new discrete duration be?
                Duration<AudioSample> truncatedDuration = _tempo->BeatsToSamples(_priorBeatDuration.AsContinuous()).RoundedUp();

                // By how many samples are we truncating?
                Duration<AudioSample> truncatedSamples = _audioStream->DiscreteDuration() - truncatedDuration;

                // And we have to truncate the audio stream since we may have recorded much more.
                _audioStream->Truncate(truncatedDuration);

                // AND we have to adjust the current playback time to compensate for truncatedSamples,
                // since those are samples the user (in retrospect) meant to start looping already.
                _localLoopTime = ContinuousTime<AudioSample>(truncatedSamples.Value());
            }
        }

        // We now need to be sample-accurate.  If we get too many samples, here is where we truncate.
        // We will capture enough samples to play back an extra sample, during loops where the loopie
        // local time fractionally wraps around.
        Duration<AudioSample> roundedUpDuration{ ExactDuration().RoundedUp() };

        // we should not have advanced beyond roundedUpDuration yet, or something went wrong at end of recording
        Duration<AudioSample> originalDiscreteDuration = _audioStream.get()->DiscreteDuration();
        Check(originalDiscreteDuration <= roundedUpDuration);

        if (originalDiscreteDuration + bufferDuration >= roundedUpDuration)
        {
            // reduce duration so we only capture the exact right number of samples
            Duration<AudioSample> captureDuration = roundedUpDuration - originalDiscreteDuration;

            // we are done recording altogether
            _state = NowSoundTrackState::TrackLooping;

            // This is the only time this variable is ever set to true.
            // The message thread polls tracks and checks this variable; any that have it set to true will
            // get their input connection removed (by the message thread, naturally, as only it can change
            // graph connections), and this will be set back to false, never to change again.  (Tracks can
            // only be in recording state when initially created.)
            // Hence there is no need for interlocking or other synchronization on this variable even though it
            // is being used for inter-thread communication.
            // TODONEXT: actually remove input connections by polling! (NYI atm)
            _justStoppedRecording = true;

            // Getting data for channel 0 is always correct, because the JUCE per-channel connections handle
            // the input-channel-to-track routing.
            _audioStream.get()->Append(captureDuration, audioBuffer.getReadPointer(0));

            // now that we have done our final append, shut the stream at the current duration.
            // This requires that the stream has captured exactly roundedUpDuration samples in total,
            // or an assertion will fire.
            _audioStream.get()->Shut(ExactDuration(), /* fade: */true);
        }
        else
        {
            // capture the full duration
            _audioStream.get()->Append(bufferDuration, audioBuffer.getReadPointer(0));
        }

        // Quiet the output audio.
        // We will start looping on the next block.
        // This avoids clicking at the start of the loop.
        for (int i = 0; i < this->getTotalNumOutputChannels(); i++)
        {
            zeromem(audioBuffer.getWritePointer(i), sizeof(float) * bufferDuration.Value());
        }
    }

    void NowSoundTrackAudioProcessor::HandleTrackLooping(NowSound::Duration<NowSound::AudioSample>& bufferDuration, juce::AudioSampleBuffer& audioBuffer, NowSound::Duration<NowSound::AudioSample>& completedDuration, juce::MidiBuffer& midiBuffer)
    {
        // The fractional part of the track's continuous duration.
        // Used when computing whether to pick up a rounded-up sample when wrapping around.
        ContinuousDuration<AudioSample> streamDuration = _audioStream.get()->ExactDuration();
        float streamFractionalDuration = streamDuration.Value() - std::floor(streamDuration.Value());

        // Copy our audio stream data into audioBuffer, slice by slice.
        while (bufferDuration > 0)
        {
            float fractionalLocalLoopTime = _localLoopTime.Value() - std::floor(_localLoopTime.Value());

            // Are we playing forwards or backwards?
            Slice<AudioSample, float> slice(
                _audioStream.get()->GetSliceIntersecting(
                    Interval<AudioSample>(_localLoopTime.RoundedDown(), bufferDuration, _direction)));

            if (_direction == Direction::Forwards)
            {
                // Is this the last slice in the stream?
                // If so, then its final offset will be equal to the stream's DiscreteDuration.
                Duration<AudioSample> sliceEnd = slice.Offset() + slice.SliceDuration();
                bool isLastSlice = sliceEnd == _audioStream.get()->DiscreteDuration();

                // If this is the last slice, then we are about to wrap around.
                // When doing this, we need to decide whether to pick up an extra rounded sample.
                // Since the DiscreteDuration is equal to the rounded-up ContinuousDuration,
                // we by default *will* get a rounded-up extra sample.
                // So now is when we decide to possibly *not* do that.
                if (isLastSlice)
                {
                    // If the fractional part of _localLoopTime plus streamFractionalDuration is
                    // less than 1, it means we are NOT rounding up, and we want to drop the
                    // rounded-up sample from the last slice.
                    float fractionalSum = fractionalLocalLoopTime + streamFractionalDuration;
                    if (fractionalSum < 1)
                    {
                        // drop the extra sample from the slice
                        slice = slice.SubsliceOfDuration(slice.SliceDuration() - Duration<AudioSample>(1));
                    }

                    // because we wrapped around and just accounted for any rounding up,
                    // we keep the fractional part only but go back to the zeroth sample
                    _localLoopTime = fractionalSum - std::floor(fractionalSum);
                }
                else
                {
                    // we can just use the slice as is.
                    // increase the local loop time by this whole slice
                    _localLoopTime = _localLoopTime + slice.SliceDuration().AsContinuous();
                }

                // copy the same audio to both output channels; it will get panned by SpatialAudioProcessor::processBlock
                slice.CopyTo(audioBuffer.getWritePointer(0) + completedDuration.Value());
                slice.CopyTo(audioBuffer.getWritePointer(1) + completedDuration.Value());
            }
            else
            {
                // here we go backwards!

                // Is this the first slice in the stream?
                // If so, then its final offset will be equal to the stream's DiscreteDuration.
                bool isFirstSlice = slice.Offset() == 0;

                // If this is the first slice, then we are about to wrap around.
                // When doing this, we need to decide whether to pick up an extra rounded sample.
                // Since the DiscreteDuration is equal to the rounded-up ContinuousDuration,
                // we by default *will* get a rounded-up extra sample.
                // So now is when we decide to possibly *not* do that.
                if (isFirstSlice)
                {
                    // The default local loop time is the whole stream, plus the fractional accumulation.
                    _localLoopTime =
                        _audioStream.get()->DiscreteDuration().Value() - 1
                        + (fractionalLocalLoopTime - streamFractionalDuration);

                    // If the fractional part of localLoopTime is more than streamFractionalDuration,
                    // then we did not in fact round up this time, and we should drop one sample.
                    if (fractionalLocalLoopTime >= streamFractionalDuration)
                    {
                        // The new local loop time keeps the fractional result, but drops the
                        // last sample.
                        _localLoopTime = ContinuousTime<AudioSample>(_localLoopTime.Value() - 1);
                    }
                }
                else
                {
                    // we can just use the slice as is.
                    // decrease the local loop time by this whole slice
                    _localLoopTime = ContinuousTime<AudioSample>(_localLoopTime.Value() - slice.SliceDuration().AsContinuous().Value());
                }

                // copy the same audio BACKWARDS to both output channels;
                // it will get panned by SpatialAudioProcessor::processBlock
                for (int sourceIndex = 0; sourceIndex < slice.SliceDuration().Value(); sourceIndex++)
                {
                    int destIndex = slice.SliceDuration().Value() - sourceIndex - 1 + completedDuration.Value();
                    audioBuffer.getWritePointer(0)[destIndex] = slice.Get(sourceIndex, 0);
                    audioBuffer.getWritePointer(1)[destIndex] = slice.Get(sourceIndex, 0);
                }
            }

            bufferDuration = bufferDuration - slice.SliceDuration();
            completedDuration = completedDuration + slice.SliceDuration();
        }

        // Now process the whole block to the output.
        // Note that this is the right thing to do even if we are looping over only a partial block;
        // the portion of the block when we were still recording will be zeroed out properly.
        SpatialAudioProcessor::processBlock(audioBuffer, midiBuffer);
    }
}
