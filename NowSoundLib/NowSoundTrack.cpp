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

#include "winrt/Windows.Foundation.h"

// From https://gist.github.com/kennykerr/f1d941c2d26227abbf762481bcbd84d3
struct __declspec(uuid("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d")) __declspec(novtable) IMemoryBufferByteAccess : ::IUnknown
{
    virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
};

namespace NowSound
{
    NowSoundTrack::NowSoundTrack(AudioInputId inputId)
        : _sequenceNumber(s_sequenceNumber++),
        _state(TrackState::Recording),
        _startTime(Clock::Instance().Now()),
        _audioStream(_startTime, NowSoundGraph::GetAudioAllocator(), 2, /*maxBufferedDuration:*/ 0, /*useContinuousLoopingMapper*/ false),
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
    
    TrackState NowSoundTrack::State() const { return _state; }
    
    Duration<Beat> NowSoundTrack::BeatDuration() const { return _beatDuration; }
    
    ContinuousDuration<Beat> NowSoundTrack::BeatPositionUnityNow() const
    {
        // TODO: determine whether we really need a time that only moves forward between Unity frames.
        // For now, let time be determined solely by audio graph, and let Unity observe time increasing 
        // during a single Unity frame.
        Duration<AudioSample> sinceStart(Clock::Instance().Now() - _startTime);
        Time<AudioSample> sinceStartTime(sinceStart.Value());

        int completeBeatsSinceStart = (int)sinceStartTime.CompleteBeats % (int)BeatDuration().Value();
        return (ContinuousDuration<Beat>)(completeBeatsSinceStart + (float)sinceStartTime.FractionalBeat);
    }

    ContinuousDuration<AudioSample> NowSoundTrack::ExactDuration() const
    { return (int)BeatDuration().Value() * Clock::Instance().BeatDuration().Value(); }

    Time<AudioSample> NowSoundTrack::StartTime() const { return _startTime; }

    bool NowSoundTrack::IsMuted() const { return _isMuted; }
    void NowSoundTrack::SetIsMuted(bool isMuted) { _isMuted = isMuted; }

    void NowSoundTrack::FinishRecording()
    {
        // TODO: ThreadContract.RequireUnity();

        // no need for any synchronization at all; the Record() logic will see this change.
        // We have no memory fence here but this write does reliably get seen sufficiently quickly in practice.
        _state = TrackState::FinishRecording;
    }

    void NowSoundTrack::Delete()
    {
        // TODO: ThreadContract.RequireUnity();

        while (_audioFrameInputNode.OutgoingConnections.Count > 0)
        {
            _audioFrameInputNode.RemoveOutgoingConnection(_audioFrameInputNode.OutgoingConnections[0].Destination);
        }
        // TODO: does destruction properly clean this up? _audioFrameInputNode.Dispose();
    }

    void NowSoundTrack::UnityUpdate()
    {
    }

    void NowSoundTrack::FrameInputNode_QuantumStarted(AudioFrameInputNode sender, FrameInputNodeQuantumStartedEventArgs args)
    {
        DateTime dateTimeNow = DateTime::clock::now();

        if (IsMuted)
        {
            // copy nothing to anywhere
            return;
        }

        if (_state == TrackState::Looping)
        {
            // we are looping; let's play!
            // TODO: why did we have this lock statement here?  lock(this)
            Check(args.RequiredSamples() >= 0);
            uint32_t requiredSamples = (uint32_t)args.RequiredSamples();

            if (requiredSamples == 0)
            {
                s_zeroByteOutgoingFrameCount++;
                return;
            }

            Windows::Media::AudioBuffer buffer(s_audioFrame.LockBuffer(Windows::Media::AudioBufferAccessMode::Write));
            IMemoryBufferReference reference(buffer.CreateReference());

            uint8_t* dataInBytes{};
            uint32_t capacityInBytes{};

            // OMG KENNY KERR WINS AGAIN:
            // https://gist.github.com/kennykerr/f1d941c2d26227abbf762481bcbd84d3e

            winrt::impl::com_ref<IMemoryBufferByteAccess> interop = reference.as<IMemoryBufferByteAccess>();
            check_hresult(interop->GetBuffer(&dataInBytes, &capacityInBytes));

            // To use low latency pipeline on every quantum, have this use requiredSamples rather than capacityInBytes.
            uint32_t bytesRemaining = capacityInBytes;
            int samplesRemaining = (int)bytesRemaining / 8; // stereo float

            
            s_audioFrame.Duration(TimeSpan(samplesRemaining * Clock::TicksPerSecond / Clock::SampleRateHz));

            while (samplesRemaining > 0)
            {
                // get up to one second or samplesRemaining, whichever is smaller
                Slice<AudioSample, float> longest(
                    _audioStream.GetNextSliceAt(Interval<AudioSample>(_localTime, samplesRemaining)));

                longest.CopyTo(reinterpret_cast<float*>(dataInBytes));

                Time<AudioSample> now();

                TimeSpan sinceLast = dateTimeNow - _lastQuantumTime;

                //string line = $"track #{_sequenceNumber}: reqSamples {requiredSamples}; {sinceLast.TotalMilliseconds} msec since last; {s_audioFrame.Duration.Value.TotalMilliseconds} msec audio frame; now {now}, _localTime {_localTime}, samplesRemaining {samplesRemaining}, slice {longest}";
                //HoloDebug.Log(line);
                //Spam.Audio.WriteLine(line);

                dataInBytes += longest.SizeInBytes();
                _localTime = _localTime + longest._duration;
                samplesRemaining -= (int)longest._duration.Value();
            }

            _audioFrameInputNode.AddFrame(s_audioFrame);

            _lastQuantumTime = dateTimeNow;
        }
    }

        // Handle incoming audio data; manage the Recording -> FinishRecording and FinishRecording -> Looping state transitions.
        bool NowSoundTrack::Record(Time<AudioSample> now, Duration<AudioSample> duration, IntPtr data)
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
                    Duration<Beat> completeBeats = Clock.Instance.DurationToTime<AudioSample>(_audioStream.DiscreteDuration + duration).CompleteBeats;

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
