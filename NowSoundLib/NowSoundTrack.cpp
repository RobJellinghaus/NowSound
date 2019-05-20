// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include <string>
#include <sstream>

#include "stdint.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "Clock.h"
#include "GetBuffer.h"
#include "MagicConstants.h"
#include "NowSoundGraph.h"
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
    std::map<TrackId, juce::AudioProcessorGraph::Node::Ptr> NowSoundTrackAudioProcessor::s_tracks{};

    void NowSoundTrackAudioProcessor::DeleteTrack(TrackId trackId)
    {
        Check(trackId >= TrackId::TrackIdUndefined && trackId <= s_tracks.size());
        Track(trackId)->Delete();
        // emplace a null pointer
        s_tracks[trackId] = nullptr;
    }

    void NowSoundTrackAudioProcessor::AddTrack(TrackId id, juce::AudioProcessorGraph::Node::Ptr track)
    {
        s_tracks.emplace(id, std::move(track));
    }

    NowSoundTrackAudioProcessor* NowSoundTrackAudioProcessor::Track(TrackId id)
    {
        // NOTE THAT THIS PATTERN DOES NOT LOCK THE _tracks COLLECTION IN ANY WAY.
        // The only way this will be correct is if all modifications to _tracks happen only as a result of
        // non-concurrent, serialized external calls to NowSoundTrackAPI.
        Check(id > TrackId::TrackIdUndefined);
        NowSoundTrackAudioProcessor* value = static_cast<NowSoundTrackAudioProcessor*>(s_tracks.at(id)->getProcessor());
        Check(value != nullptr); // TODO: don't fail on invalid client values; instead return standard error code or something
        return value;
    }

	bool NowSoundTrackAudioProcessor::TrackIsDefined(TrackId id)
	{
		// Race conditions can lead to a track being checked before it actually exists.
		// TODO: THIS SHOULD NOT BE THE CASE AND SHOULD BE FIXED.
		// For now, nonetheless, let's try this workaround and verify if it happens.
		return s_tracks.find(id) != s_tracks.end();
	}

    NowSoundTrackAudioProcessor::NowSoundTrackAudioProcessor(
		NowSoundGraph* graph,
        TrackId trackId,
        const BufferedSliceStream<AudioSample, float>& sourceStream,
		float initialPan)
		: SpatialAudioProcessor(graph, initialPan),
		_trackId{ trackId },
        _state{ NowSoundTrackState::TrackRecording },
        // latency compensation effectively means the track started before it was constructed ;-)
        _audioStream(
            Clock::Instance().Now() - Clock::Instance().TimeToSamples(MagicConstants::PreRecordingDuration),
            1, // mono streams only for now (and maybe indefinitely)
            NowSoundGraph::Instance()->AudioAllocator(),
            /*maxBufferedDuration:*/ 0,
            /*useContinuousLoopingMapper*/ false),
        // one beat is the shortest any track ever is (TODO: allow optionally relaxing quantization)
        _beatDuration{ 1 },
        _lastSampleTime{ Clock::Instance().Now() }
	{
        Check(_lastSampleTime.Value() >= 0);

        // Tracks should only be created from the UI thread (or at least not from the audio thread).
        // TODO: thread contracts.

        // should only ever call this when graph is fully up and running
        Check(NowSoundGraph::Instance()->State() == NowSoundGraphState::GraphRunning);

		/* HACK: try NOT pre-recording any data... just push the start time back
        if (MagicConstants::PreRecordingDuration.Value() > 0)
        {
            // Prepend latencyCompensation's worth of previously buffered input audio, to prepopulate this track.
			Duration<AudioSample> latencyCompensationDuration = Clock::Instance().TimeToSamples(MagicConstants::PreRecordingDuration);
            Interval<AudioSample> lastIntervalOfSourceStream(
                sourceStream.InitialTime() + sourceStream.DiscreteDuration() - latencyCompensationDuration,
                latencyCompensationDuration);
            sourceStream.AppendTo(lastIntervalOfSourceStream, &_audioStream);
        }
		*/

		std::wstringstream wstr{};
		wstr << L"NowSoundTrack::NowSoundTrack(" << trackId << L")";
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

    NowSoundTrackState NowSoundTrackAudioProcessor::State() const { return _state; }
    
    Duration<Beat> NowSoundTrackAudioProcessor::BeatDuration() const { return _beatDuration; }
    
    ContinuousDuration<Beat> NowSoundTrackAudioProcessor::BeatPositionUnityNow() const
    {
        // TODO: determine whether we really need a time that only moves forward between Unity frames.
        // For now, let time be determined solely by audio graph, and let Unity observe time increasing 
        // during a single Unity frame.
        Duration<AudioSample> sinceStart(Clock::Instance().Now() - _audioStream.InitialTime());
        Time<AudioSample> sinceStartTime(sinceStart.Value());

        ContinuousDuration<Beat> beats = Clock::Instance().TimeToBeats(sinceStartTime);
        int completeBeatsSinceStart = (int)beats.Value() % (int)BeatDuration().Value();
        return (ContinuousDuration<Beat>)(completeBeatsSinceStart + (beats.Value() - (int)beats.Value()));
    }

    ContinuousDuration<AudioSample> NowSoundTrackAudioProcessor::ExactDuration() const
    {
        return (int)BeatDuration().Value() * Clock::Instance().BeatDuration().Value();
    }

    Time<AudioSample> NowSoundTrackAudioProcessor::StartTime() const { return _audioStream.InitialTime(); }

    ContinuousDuration<Beat> TrackBeats(Duration<AudioSample> localTime, Duration<Beat> beatDuration)
    {
        ContinuousDuration<Beat> totalBeats = Clock::Instance().TimeToBeats(localTime.Value());
        Duration<Beat> nonFractionalBeats((int)totalBeats.Value());

        return (ContinuousDuration<Beat>)(
            // total (non-fractional) beats modulo the beat duration of the track
            (nonFractionalBeats.Value() % beatDuration.Value())
            // fractional beats of the track
            + (totalBeats.Value() - nonFractionalBeats.Value()));
    }

    NowSoundTrackInfo NowSoundTrackAudioProcessor::Info() 
    {
        Time<AudioSample> lastSampleTime = this->_lastSampleTime; // to prevent any drift from this being updated concurrently
        Time<AudioSample> startTime = this->_audioStream.InitialTime();
		Duration<AudioSample> localClockTime = Clock::Instance().Now() - startTime;
        return CreateNowSoundTrackInfo(
            _state == NowSoundTrackState::TrackLooping,
            startTime.Value(),
            Clock::Instance().TimeToBeats(startTime).Value(),
            this->_audioStream.DiscreteDuration().Value(),
            this->BeatDuration().Value(),
            this->_state == NowSoundTrackState::TrackLooping ? _audioStream.ExactDuration().Value() : 0,
			localClockTime.Value(),
			TrackBeats(localClockTime, this->_beatDuration).Value(),
			(lastSampleTime - startTime).Value(),
			Pan());
    }

	void NowSoundTrackAudioProcessor::FinishRecording()
    {
        // TODO: ThreadContract.RequireUnity();

        // no need for any synchronization at all; the Record() logic will see this change.
        // We have no memory fence here but this write does reliably get seen sufficiently quickly in practice.
        _state = NowSoundTrackState::TrackFinishRecording;
    }

    void NowSoundTrackAudioProcessor::Delete()
    {
        // TODO: ThreadContract.RequireUnity();

        // _audioFrameInputNode.Stop();

		/*
        while (_audioFrameInputNode.OutgoingConnections().Size() > 0)
        {
            _audioFrameInputNode.RemoveOutgoingConnection(_audioFrameInputNode.OutgoingConnections().GetAt(0).Destination());
        }
        // TODO: does destruction properly clean this up? _audioFrameInputNode.Dispose();
		*/
    }

    void NowSoundTrackAudioProcessor::processBlock(AudioBuffer<float>& audioBuffer, MidiBuffer& midiBuffer)
    {
        // This should always take two channels.  Only channel 0 is used on input.  Both channels are used
        // on output (only stereo supported for now).
        Check(audioBuffer.getNumChannels() == 2);

        // Depending on the current state of this track, we either record, or we finish recording
        // and switch modes to looping, or we're straight looping.
        Duration<AudioSample> bufferDuration{ audioBuffer.getNumSamples() };
        // The duration that's been copied to the input channel so far (used when looping).
        Duration<AudioSample> completedDuration{ 0 };

        switch (_state)
        {
        case NowSoundTrackState::TrackRecording:
        {
            // How many complete beats after we record this data?
            Time<AudioSample> durationAsTime((_audioStream.DiscreteDuration() + bufferDuration).Value());
            Duration<Beat> completeBeats = (Duration<Beat>)((int)Clock::Instance().TimeToBeats(durationAsTime).Value());

            // If it's more than our _beatDuration, bump our _beatDuration
            // TODO: implement other quantization policies here
            if (completeBeats >= _beatDuration)
            {
                // 1/2/4* quantization, like old times. TODO: make this selectable
                if (_beatDuration == 1)
                {
                    _beatDuration = 2;
                }
                else if (_beatDuration == 2)
                {
                    _beatDuration = 4;
                }
                else
                {
                    _beatDuration = _beatDuration + Duration<Beat>(4);
                }
                // blow up if we happen somehow to be recording more than one beat's worth (should never happen given low latency expectation)
                Check(completeBeats < BeatDuration());
            }

            // and actually record the full amount of available data.
            // Getting data for channel 0 is always correct because the JUCE per-channel connections handle which
            // input channel goes to which track.
            _audioStream.Append(bufferDuration, audioBuffer.getReadPointer(0));

            // and step on all output channels
            for (int i = 0; i < this->getTotalNumOutputChannels(); i++)
            {
                zeromem(audioBuffer.getWritePointer(i), sizeof(float) * bufferDuration.Value());
            }
            break;
        }

        case NowSoundTrackState::TrackFinishRecording:
        {
            // we now need to be sample-accurate.  If we get too many samples, here is where we truncate.
            Duration<AudioSample> roundedUpDuration((long)std::ceil(ExactDuration().Value()));

            // we should not have advanced beyond roundedUpDuration yet, or something went wrong at end of recording
            Duration<AudioSample> originalDiscreteDuration = _audioStream.DiscreteDuration();
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

                // Getting data for channel 0 is always correct because the JUCE per-channel connections handle which
                // input channel goes to which track.
                _audioStream.Append(captureDuration, audioBuffer.getReadPointer(0));

                // now that we have done our final append, shut the stream at the current duration
                _audioStream.Shut(ExactDuration());
            }
            else
            {
                // capture the full duration
                _audioStream.Append(bufferDuration, audioBuffer.getReadPointer(0));
            }

            // zero the output audio altogether.
            // we will start looping on the next block.
            for (int i = 0; i < this->getTotalNumOutputChannels(); i++)
            {
                zeromem(audioBuffer.getWritePointer(i), sizeof(float) * bufferDuration.Value());
            }

            // and break
            break;
        }

        case NowSoundTrackState::TrackLooping:
        {
            while (bufferDuration > 0)
            {
                // copy the correct track audio into the audio buffer's channel 0
                Slice<AudioSample, float> slice(
                    _audioStream.GetSliceContaining(Interval<AudioSample>(_lastSampleTime, bufferDuration)));
                // Slice duration may be less than remaining duration if the internal stream buffer was short
                Duration<AudioSample> sliceDuration = slice.SliceDuration();

                slice.CopyTo(audioBuffer.getWritePointer(0) + completedDuration.Value());

                bufferDuration = bufferDuration - sliceDuration;
                completedDuration = completedDuration + sliceDuration;
                _lastSampleTime = _lastSampleTime + sliceDuration;
            }

            // Now process the whole block to the output.
            // Note that this is the right thing to do even if we are looping over only a partial block;
            // the portion of the block when we were still recording will be zeroed out properly.
            SpatialAudioProcessor::processBlock(audioBuffer, midiBuffer);

            break;
        }
        }
    }
}
