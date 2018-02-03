// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include "stdint.h"

#include "BufferAllocator.h"
#include "Clock.h"
#include "NowSoundLib.h"
#include "NowSoundTrack.h"
#include "Recorder.h"
#include "Slice.h"
#include "SliceStream.h"
#include "Time.h"

using namespace NowSound;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace Windows::Foundation;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Render;

namespace NowSound
{
    NowSoundTrack::NowSoundTrack(AudioInputId inputId)
        : _sequenceNumber(s_sequenceNumber++),
        _state(TrackState::Recording),
        _startMoment(),
        _audioStream(_startMoment.Time(), NowSoundGraph::GetAudioAllocator(), 2, /*maxBufferedDuration:*/ 0, /*useContinuousLoopingMapper*/ false),
        // one beat is the shortest any track ever is
        _beatDuration(1),
        _audioFrameInputNode(NowSoundGraph::GetAudioGraph().CreateFrameInputNode()),
        _localTime(0)
    {
        // Tracks should only be created from the Unity thread.
        // TODO: thread contracts.

        // should only ever call this when graph is fully up and running
        Check(NowSoundGraph::NowSoundGraph_GetGraphState() == NowSoundGraph_State::Running);

        // Now is when we create the AudioFrameInputNode, because now is when we are sure we are not on the
        // audio thread.
        _audioFrameInputNode.AddOutgoingConnection(NowSoundGraph::GetAudioDeviceOutputNode());
        _audioFrameInputNode.QuantumStarted(FrameInputNode_QuantumStarted);
    }
    
    TrackState NowSoundTrack::State() { return _state; }
    
    Duration<Beat> NowSoundTrack::BeatDuration() { return _beatDuration; }
    
    ContinuousDuration<Beat> NowSoundTrack::BeatPositionUnityNow()
    {
        Duration<AudioSample> sinceStart = Clock::UnityNow()::Time - _startMoment.Time;
        Moment sinceStartMoment = Clock::Instance.DurationToMoment(sinceStart);

        int completeBeatsSinceStart = (int)sinceStartMoment.CompleteBeats % (int)BeatDuration;
        return (ContinuousDuration<Beat>)(completeBeatsSinceStart + (float)sinceStartMoment.FractionalBeat);
    }

            // 
            // How long is this track?
            // 
            // 
            // This is increased during recording.  It may in general have fractional numbers of samples.
            // </remarks>
        ContinuousDuration<AudioSample> NowSoundTrack::Duration = > (int)BeatDuration * Clock.Instance.BeatDuration;

        // 
        // The starting moment at which this Track was created.
        // 
        Moment NowSoundTrack::StartMoment = > _startMoment;

        // 
        // True if this is muted.
        // 
        // 
        // Note that something can be in FinishRecording state but still be muted, if the user is fast!
        // Hence this is a separate flag, not represented as a TrackState.
        // </remarks>
        bool NowSoundTrack::IsMuted{ get; set; }

        DenseSampleFloatStream NowSoundTrack::Stream = > throw new NotImplementedException();

            // 
            // The user wishes the track to finish recording now.
            // 
            // 
            // Contractually requires State == TrackState.Recording.
            // </remarks>
            void NowSoundTrack::FinishRecording()
        {
            ThreadContract.RequireUnity();

            // no need for any synchronization at all; the Record() logic will see this change
            _state = TrackState.FinishRecording;
        }

        // 
        // Delete this Track; after this, all methods become invalid to call (contract failure).
        // 
        void NowSoundTrack::Delete()
        {
            ThreadContract.RequireUnity();

            while (_audioFrameInputNode.OutgoingConnections.Count > 0)
            {
                _audioFrameInputNode.RemoveOutgoingConnection(_audioFrameInputNode.OutgoingConnections[0].Destination);
            }
            _audioFrameInputNode.Dispose();
        }

        // 
        // Update the track's state as applicable from Unity thread.
        // 
        // 
        // If there is any coordination that is needed from outside the audio graph thread, this method implements it.
        // </remarks>
        void NowSoundTrack::UnityUpdate()
        {
        }

        void NowSoundTrack::FrameInputNode_QuantumStarted(AudioFrameInputNode sender, FrameInputNodeQuantumStartedEventArgs args)
        {
            DateTime dateTimeNow = DateTime::Now();

            if (IsMuted)
            {
                // copy nothing to anywhere
                return;
            }

            if (_state == TrackState.Looping)
            {
                // we are looping; let's play!
                lock(this)
                {
                    uint requiredSamples = (uint)args.RequiredSamples;

                    if (requiredSamples == 0)
                    {
                        s_zeroByteOutgoingFrameCount++;
                        return;
                    }

                    using (AudioBuffer buffer = s_audioFrame.LockBuffer(AudioBufferAccessMode.Write))
                        using (IMemoryBufferReference reference = buffer.CreateReference())
                    {
                        byte* dataInBytes;
                        uint capacityInBytes;

                        // Get the buffer from the AudioFrame
                        ((IMemoryBufferByteAccess)reference).GetBuffer(out dataInBytes, out capacityInBytes);

                        // To use low latency pipeline on every quantum, have this use requiredSamples rather than capacityInBytes.
                        uint bytesRemaining = capacityInBytes;
                        int samplesRemaining = (int)bytesRemaining / 8; // stereo float

                        s_audioFrame.Duration = TimeSpan.FromSeconds(samplesRemaining / Clock.SampleRateHz);

                        while (samplesRemaining > 0)
                        {
                            // get up to one second or samplesRemaining, whichever is smaller
                            Slice<AudioSample, float> longest = _audioStream.GetNextSliceAt(new Interval<AudioSample>(_localTime, samplesRemaining));

                            longest.CopyToIntPtr(new IntPtr(dataInBytes));

                            Moment now = Clock.Instance.AudioNow;

                            TimeSpan sinceLast = dateTimeNow - _lastQuantumTime;


                            //string line = $"track #{_sequenceNumber}: reqSamples {requiredSamples}; {sinceLast.TotalMilliseconds} msec since last; {s_audioFrame.Duration.Value.TotalMilliseconds} msec audio frame; now {now}, _localTime {_localTime}, samplesRemaining {samplesRemaining}, slice {longest}";
                            //HoloDebug.Log(line);
                            //Spam.Audio.WriteLine(line);


                            dataInBytes += longest.Duration * sizeof(float) * longest.SliverSize;
                            _localTime += longest.Duration;
                            samplesRemaining -= (int)longest.Duration;
                        }
                    }

                    _audioFrameInputNode.AddFrame(s_audioFrame);
                }

                _lastQuantumTime = dateTimeNow;
            }
        }

        // 
        // Handle incoming audio data; manage the Recording -> FinishRecording and FinishRecording -> Looping state transitions.
        // 
        bool NowSoundTrack::Record(Moment now, Duration<AudioSample> duration, IntPtr data)
        {
            ThreadContract.RequireAudioGraph();

            bool continueRecording = true;
            lock(this)
            {
                switch (_state)
                {
                case TrackState.Recording:
                {
                    // How many complete beats after we record this data?
                    Duration<Beat> completeBeats = Clock.Instance.DurationToMoment(_audioStream.DiscreteDuration + duration).CompleteBeats;

                    // If it's more than our _beatDuration, bump our _beatDuration
                    // TODO: implement other quantization policies here
                    if (completeBeats >= _beatDuration)
                    {
                        _beatDuration = completeBeats + 1;
                        // blow up if we happen somehow to be recording more than one beat's worth (should never happen given low latency expectation)
                        Check(completeBeats < BeatDuration, "completeBeats < BeatDuration");
                    }

                    // and actually record the full amount of available data
                    _audioStream.Append(duration, data);

                    break;
                }

                case TrackState.FinishRecording:
                {
                    // we now need to be sample-accurate.  If we get too many samples, here is where we truncate.
                    Duration<AudioSample> roundedUpDuration = (long)Math.Ceiling((float)Duration);

                    // we should not have advanced beyond roundedUpDuration yet, or something went wrong at end of recording
                    Check(_audioStream.DiscreteDuration <= roundedUpDuration, "_localTime <= roundedUpDuration");

                    if (_audioStream.DiscreteDuration + duration >= roundedUpDuration)
                    {
                        // reduce duration so we only capture the exact right number of samples
                        duration = roundedUpDuration - _audioStream.DiscreteDuration;

                        // we are done recording altogether
                        _state = TrackState.Looping;
                        continueRecording = false;

                        _audioStream.Append(duration, data);

                        // now that we have done our final append, shut the stream at the current duration
                        _audioStream.Shut(Duration);

                        // and initialize _localTime
                        _localTime = now.Time;
                    }
                    else
                    {
                        // capture the full duration
                        _audioStream.Append(duration, data);
                    }

                    break;
                }

                case TrackState.Looping:
                {
                    Contract.Fail("Should never still be recording once in looping state");
                    return false;
                }
                }
            }

            return continueRecording;
        }
    };

    Windows::Media::AudioFrame NowSoundTrack::s_audioFrame(Clock::SampleRateHz / 4 * sizeof(float) * 2);

}
