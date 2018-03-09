// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include "stdint.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "Clock.h"
#include "GetBuffer.h"
#include "NowSoundGraph.h"
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
    __declspec(dllexport) NowSoundTrackState NowSoundTrackAPI::NowSoundTrack_State(TrackId trackId)
    {
        return Track(trackId)->State();
    }

    __declspec(dllexport) int64_t /*Duration<Beat>*/ NowSoundTrackAPI::NowSoundTrack_BeatDuration(TrackId trackId)
    {
        return Track(trackId)->BeatDuration().Value();
    }

    __declspec(dllexport) float /*ContinuousDuration<Beat>*/ NowSoundTrackAPI::NowSoundTrack_BeatPositionUnityNow(TrackId trackId)
    {
        return Track(trackId)->BeatPositionUnityNow().Value();
    }

    __declspec(dllexport) float /*ContinuousDuration<AudioSample>*/ NowSoundTrackAPI::NowSoundTrack_ExactDuration(TrackId trackId)
    {
        return Track(trackId)->ExactDuration().Value();
    }

    __declspec(dllexport) int64_t /*Time<AudioSample>*/ NowSoundTrackAPI::NowSoundTrack_StartTime(TrackId trackId)
    {
        return Track(trackId)->StartTime().Value();
    }

    __declspec(dllexport) void NowSoundTrackAPI::NowSoundTrack_FinishRecording(TrackId trackId)
    {
        Track(trackId)->FinishRecording();
    }

    __declspec(dllexport) bool NowSoundTrackAPI::NowSoundTrack_IsMuted(TrackId trackId)
    {
        return Track(trackId)->IsMuted();
    }

    __declspec(dllexport) void NowSoundTrackAPI::NowSoundTrack_SetIsMuted(TrackId trackId, bool isMuted)
    {
        Track(trackId)->SetIsMuted(isMuted);
    }

    __declspec(dllexport) void NowSoundTrackAPI::NowSoundTrack_Delete(TrackId trackId)
    {
        Track(trackId)->Delete();
        // emplace a null pointer
        _tracks[static_cast<int>(trackId)] = std::unique_ptr<NowSoundTrack>{};
    }

    void __declspec(dllexport) NowSoundTrackAPI::NowSoundTrack_UnityUpdate(TrackId trackId)
    {
        Track(trackId)->UnityUpdate();
    }

    std::map<TrackId, std::unique_ptr<NowSoundTrack>> NowSoundTrackAPI::_tracks{};

    NowSoundTrack* NowSoundTrackAPI::Track(TrackId id)
    {
        NowSoundTrack* value = _tracks.at(id).get();
        Check(value != nullptr); // TODO: don't fail on invalid client values; instead return standard error code or something
        return value;
    }

    NowSoundTrack::NowSoundTrack(TrackId trackId, AudioInputId inputId)
        : _trackId(trackId),
            _inputId(inputId),
            _state(NowSoundTrackState::Recording),
            _startTime(Clock::Instance().Now()),
            _audioStream(
                _startTime,
                2,
                NowSoundGraph::Instance()->GetAudioAllocator(),
                /*maxBufferedDuration:*/ 0,
                /*useContinuousLoopingMapper*/ false),
            // one beat is the shortest any track ever is (TODO: allow optionally relaxing quantization)
            _beatDuration(1),
            _audioFrameInputNode(NowSoundGraph::Instance()->GetAudioGraph().CreateFrameInputNode()),
            _localTime(0)
    {
        // Tracks should only be created from the UI thread (or at least not from the audio thread).
        // TODO: thread contracts.

        // should only ever call this when graph is fully up and running
        Check(NowSoundGraph::Instance()->GetGraphState() == NowSoundGraphState::Running);

        // Now is when we create the AudioFrameInputNode, because now is when we are sure we are not on the
        // audio thread.
        // TODO: is it right to add this outgoing connection now? Or should this happen when switching to playing?
        // In general, what is the most synchronous / fastest way to switch from recording to playing?
        _audioFrameInputNode.AddOutgoingConnection(NowSoundGraph::Instance()->GetAudioDeviceOutputNode());
        _audioFrameInputNode.QuantumStarted([&](AudioFrameInputNode sender, FrameInputNodeQuantumStartedEventArgs args)
        {
            FrameInputNode_QuantumStarted(sender, args);
        });


    }
    
    NowSoundTrackState NowSoundTrack::State() const { return _state; }
    
    Duration<Beat> NowSoundTrack::BeatDuration() const { return _beatDuration; }
    
    ContinuousDuration<Beat> NowSoundTrack::BeatPositionUnityNow() const
    {
        // TODO: determine whether we really need a time that only moves forward between Unity frames.
        // For now, let time be determined solely by audio graph, and let Unity observe time increasing 
        // during a single Unity frame.
        Duration<AudioSample> sinceStart(Clock::Instance().Now() - _startTime);
        Time<AudioSample> sinceStartTime(sinceStart.Value());

        ContinuousDuration<Beat> beats = Clock::Instance().TimeToBeats(sinceStartTime);
        int completeBeatsSinceStart = (int)beats.Value() % (int)BeatDuration().Value();
        return (ContinuousDuration<Beat>)(completeBeatsSinceStart + (beats.Value() - (int)beats.Value()));
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
        _state = NowSoundTrackState::FinishRecording;
    }

    void NowSoundTrack::Delete()
    {
        // TODO: ThreadContract.RequireUnity();

        while (_audioFrameInputNode.OutgoingConnections().Size() > 0)
        {
            _audioFrameInputNode.RemoveOutgoingConnection(_audioFrameInputNode.OutgoingConnections().GetAt(0).Destination());
        }
        // TODO: does destruction properly clean this up? _audioFrameInputNode.Dispose();
    }

    void NowSoundTrack::UnityUpdate()
    {
    }

    void NowSoundTrack::FrameInputNode_QuantumStarted(AudioFrameInputNode sender, FrameInputNodeQuantumStartedEventArgs args)
    {
        DateTime dateTimeNow = DateTime::clock::now();

        if (IsMuted() || _state != NowSoundTrackState::Recording)
        {
            // copy nothing to anywhere
            return;
        }

        if (_state == NowSoundTrackState::Looping)
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

            uint8_t* dataInBytes{};
            uint32_t capacityInBytes{};

            // OMG KENNY KERR WINS AGAIN:
            // https://gist.github.com/kennykerr/f1d941c2d26227abbf762481bcbd84d3
            Windows::Media::AudioBuffer buffer(NowSoundGraph::Instance()->GetAudioFrame().LockBuffer(Windows::Media::AudioBufferAccessMode::Write));
            IMemoryBufferReference reference(buffer.CreateReference());
            winrt::impl::com_ref<IMemoryBufferByteAccess> interop = reference.as<IMemoryBufferByteAccess>();
            check_hresult(interop->GetBuffer(&dataInBytes, &capacityInBytes));

            // To use low latency pipeline on every quantum, have this use requiredSamples rather than capacityInBytes.
            uint32_t bytesRemaining = capacityInBytes;
            int slicesRemaining = (int)bytesRemaining / 8; // stereo float
            
            NowSoundGraph::Instance()->GetAudioFrame().Duration(TimeSpan(slicesRemaining * Clock::TicksPerSecond / Clock::SampleRateHz));

            while (slicesRemaining > 0)
            {
                // get up to one second or samplesRemaining, whichever is smaller
                Slice<AudioSample, float> longest(
                    _audioStream.GetSliceContaining(Interval<AudioSample>(_localTime, slicesRemaining)));

                longest.CopyTo(reinterpret_cast<float*>(dataInBytes));

                Time<AudioSample> now(Clock::Instance().Now());

                TimeSpan sinceLast = dateTimeNow - _lastQuantumTime;

                //string line = $"track #{_sequenceNumber}: reqSamples {requiredSamples}; {sinceLast.TotalMilliseconds} msec since last; {NowSoundGraph::GetAudioFrame().Duration.Value.TotalMilliseconds} msec audio frame; now {now}, _localTime {_localTime}, samplesRemaining {samplesRemaining}, slice {longest}";
                //HoloDebug.Log(line);
                //Spam.Audio.WriteLine(line);

                dataInBytes += longest.SliceDuration().Value() * longest.SliverCount() * sizeof(float);
                _localTime = _localTime + longest.SliceDuration();
                slicesRemaining -= (int)longest.SliceDuration().Value();
            }

            _audioFrameInputNode.AddFrame(NowSoundGraph::Instance()->GetAudioFrame());

            _lastQuantumTime = dateTimeNow;
        }
    }

    // Handle incoming audio data; manage the Recording -> FinishRecording and FinishRecording -> Looping state transitions.
    bool NowSoundTrack::Record(Time<AudioSample> now, Duration<AudioSample> duration, float* data)
    {
        // TODO: ThreadContract.RequireAudioGraph();

        bool continueRecording = true;
        // TODO: what was this for? lock(this)
        switch (_state)
        {
        case NowSoundTrackState::Recording:
        {
            // How many complete beats after we record this data?
            Time<AudioSample> durationAsTime((_audioStream.DiscreteDuration() + duration).Value());
            Duration<Beat> completeBeats = Clock::Instance().TimeToCompleteBeats(durationAsTime);

            // If it's more than our _beatDuration, bump our _beatDuration
            // TODO: implement other quantization policies here
            if (completeBeats >= _beatDuration)
            {
                _beatDuration = completeBeats + Duration<Beat>(1);
                // blow up if we happen somehow to be recording more than one beat's worth (should never happen given low latency expectation)
                Check(completeBeats < BeatDuration());
            }

            // and actually record the full amount of available data
            _audioStream.Append(duration, data);

            break;
        }

        case NowSoundTrackState::FinishRecording:
        {
            // we now need to be sample-accurate.  If we get too many samples, here is where we truncate.
            Duration<AudioSample> roundedUpDuration((long)std::ceil(ExactDuration().Value()));

            // we should not have advanced beyond roundedUpDuration yet, or something went wrong at end of recording
            Check(_audioStream.DiscreteDuration() <= roundedUpDuration);

            if (_audioStream.DiscreteDuration() + duration >= roundedUpDuration)
            {
                // reduce duration so we only capture the exact right number of samples
                duration = roundedUpDuration - _audioStream.DiscreteDuration();

                // we are done recording altogether
                _state = NowSoundTrackState::Looping;
                continueRecording = false;

                _audioStream.Append(duration, data);

                // now that we have done our final append, shut the stream at the current duration
                _audioStream.Shut(ExactDuration());

                // and initialize _localTime
                _localTime = now;
            }
            else
            {
                // capture the full duration
                _audioStream.Append(duration, data);
            }

            break;
        }

        case NowSoundTrackState::Looping:
        {
            Check(false); // Should never still be recording once in looping state
            return false;
        }
        }

        return continueRecording;
    }

    int NowSoundTrack::s_zeroByteOutgoingFrameCount{};
}
