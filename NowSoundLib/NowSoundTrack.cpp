// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include <string>
#include <sstream>

#include "stdint.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "Clock.h"
#include "GetBuffer.h"
#include "MagicNumbers.h"
#include "NowSoundGraph.h"
#include "NowSoundLib.h"
#include "NowSoundTrack.h"
#include "Recorder.h"
#include "Slice.h"
#include "SliceStream.h"
#include "Time.h"

using namespace concurrency;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Render;
using namespace Windows::System;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;

namespace NowSound
{
    __declspec(dllexport) NowSoundTrackState NowSoundTrack_State(TrackId trackId)
    {
        return NowSoundTrack::Track(trackId)->State();
    }

    __declspec(dllexport) int64_t /*Duration<Beat>*/ NowSoundTrack_BeatDuration(TrackId trackId)
    {
        return NowSoundTrack::Track(trackId)->BeatDuration().Value();
    }

    __declspec(dllexport) float /*ContinuousDuration<Beat>*/ NowSoundTrack_BeatPositionUnityNow(TrackId trackId)
    {
        return NowSoundTrack::Track(trackId)->BeatPositionUnityNow().Value();
    }

    __declspec(dllexport) float /*ContinuousDuration<AudioSample>*/ NowSoundTrack_ExactDuration(TrackId trackId)
    {
        return NowSoundTrack::Track(trackId)->ExactDuration().Value();
    }

    __declspec(dllexport) NowSoundTrackTimeInfo NowSoundTrack_TimeInfo(TrackId trackId)
    {
        return NowSoundTrack::Track(trackId)->TimeInfo();
    }

    __declspec(dllexport) void NowSoundTrack_FinishRecording(TrackId trackId)
    {
        NowSoundTrack::Track(trackId)->FinishRecording();
    }

    __declspec(dllexport) bool NowSoundTrack_IsMuted(TrackId trackId)
    {
        return NowSoundTrack::Track(trackId)->IsMuted();
    }

    __declspec(dllexport) void NowSoundTrack_SetIsMuted(TrackId trackId, bool isMuted)
    {
        NowSoundTrack::Track(trackId)->SetIsMuted(isMuted);
    }

    __declspec(dllexport) void NowSoundTrack_Delete(TrackId trackId)
    {
        NowSoundTrack::DeleteTrack(trackId);
    }

    std::map<TrackId, std::unique_ptr<NowSoundTrack>> NowSoundTrack::_tracks{};

    void NowSoundTrack::DeleteTrack(TrackId trackId)
    {
        Check(trackId >= TrackId::Undefined && trackId <= _tracks.size());
        Track(trackId)->Delete();
        // emplace a null pointer
        _tracks[trackId] = std::unique_ptr<NowSoundTrack>{};
    }

    void NowSoundTrack::AddTrack(TrackId id, std::unique_ptr<NowSoundTrack>&& track)
    {
        _tracks.emplace(id, std::move(track));
    }

    NowSoundTrack* NowSoundTrack::Track(TrackId id)
    {
        // NOTE THAT THIS PATTERN DOES NOT LOCK THE _tracks COLLECTION IN ANY WAY.
        // The only way this will be correct is if all modifications to _tracks happen only as a result of
        // non-concurrent, serialized external calls to NowSoundTrackAPI.
        Check(id >= TrackId::Undefined);
        NowSoundTrack* value = _tracks.at(id).get();
        Check(value != nullptr); // TODO: don't fail on invalid client values; instead return standard error code or something
        return value;
    }

    NowSoundTrack::NowSoundTrack(
        TrackId trackId,
        AudioInputId inputId,
        const BufferedSliceStream<AudioSample, float>& sourceStream)
        : _trackId{ trackId },
        _inputId{ inputId },
        _state{ NowSoundTrackState::TrackRecording },
        // latency compensation effectively means the track started before it was constructed ;-)
        _audioStream(
            Clock::Instance().Now() - MagicNumbers::TrackLatencyCompensation,
            MagicNumbers::AudioChannelCount,
            NowSoundGraph::Instance()->GetAudioAllocator(),
            /*maxBufferedDuration:*/ 0,
            /*useContinuousLoopingMapper*/ false),
        // one beat is the shortest any track ever is (TODO: allow optionally relaxing quantization)
        _beatDuration{ 1 },
        _audioFrameInputNode{ NowSoundGraph::Instance()->GetAudioGraph().CreateFrameInputNode() },
        _localTime{ 0 },
        _isMuted{ false },
        _debugLog{},
        _sinceLastSampleTimingHistogram{MagicNumbers::AudioQuantumHistogramCapacity}
    {
        // Tracks should only be created from the UI thread (or at least not from the audio thread).
        // TODO: thread contracts.

        // should only ever call this when graph is fully up and running
        Check(NowSoundGraph::Instance()->GetGraphState() == NowSoundGraphState::GraphRunning);

        if (MagicNumbers::TrackLatencyCompensation > 0)
        {
            // Prepend latencyCompensation's worth of previously buffered input audio, to prepopulate this track.
            Interval<AudioSample> lastIntervalOfSourceStream(
                sourceStream.InitialTime() + sourceStream.DiscreteDuration() - MagicNumbers::TrackLatencyCompensation,
                MagicNumbers::TrackLatencyCompensation);
            sourceStream.AppendTo(lastIntervalOfSourceStream, &_audioStream);
        }

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

    void NowSoundTrack::DebugLog(const std::wstring& entry)
    {
        _debugLog.push(entry);
        if (_debugLog.size() > MagicNumbers::DebugLogCapacity)
        {
            _debugLog.pop();
        }
    }
    
    NowSoundTrackState NowSoundTrack::State() const { return _state; }
    
    Duration<Beat> NowSoundTrack::BeatDuration() const { return _beatDuration; }
    
    ContinuousDuration<Beat> NowSoundTrack::BeatPositionUnityNow() const
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

    ContinuousDuration<AudioSample> NowSoundTrack::ExactDuration() const
    {
        return (int)BeatDuration().Value() * Clock::Instance().BeatDuration().Value();
    }

    Time<AudioSample> NowSoundTrack::StartTime() const { return _audioStream.InitialTime(); }

    ContinuousDuration<Beat> TrackBeats(Time<AudioSample> localTime, Duration<Beat> beatDuration)
    {
        ContinuousDuration<Beat> totalBeats = Clock::Instance().TimeToBeats(localTime);
        Duration<Beat> nonFractionalBeats((int)totalBeats.Value());

        return (ContinuousDuration<Beat>)(
            // total (non-fractional) beats modulo the beat duration of the track
            (nonFractionalBeats.Value() % beatDuration.Value())
            // fractional beats of the track
            + (totalBeats.Value() - nonFractionalBeats.Value()));
    }

    NowSoundTrackTimeInfo NowSoundTrack::TimeInfo() const
    {
        Time<AudioSample> localTime = this->_localTime; // to prevent any drift from this being updated concurrently
        return CreateNowSoundTrackTimeInfo(
            this->_audioStream.InitialTime().Value(),
            this->_audioStream.DiscreteDuration().Value(),
            this->BeatDuration().Value(),
            this->_state == NowSoundTrackState::TrackLooping ? _audioStream.ExactDuration().Value() : 0,
            localTime.Value(),
            TrackBeats(localTime, this->_beatDuration).Value(),
            _sinceLastSampleTimingHistogram.Min(),
            _sinceLastSampleTimingHistogram.Max(),
            _sinceLastSampleTimingHistogram.Average());
    }

    bool NowSoundTrack::IsMuted() const { return _isMuted; }
    void NowSoundTrack::SetIsMuted(bool isMuted) { _isMuted = isMuted; }

    void NowSoundTrack::FinishRecording()
    {
        // TODO: ThreadContract.RequireUnity();

        // no need for any synchronization at all; the Record() logic will see this change.
        // We have no memory fence here but this write does reliably get seen sufficiently quickly in practice.
        _state = NowSoundTrackState::TrackFinishRecording;
    }

    void NowSoundTrack::Delete()
    {
        // TODO: ThreadContract.RequireUnity();

        _audioFrameInputNode.Stop();

        while (_audioFrameInputNode.OutgoingConnections().Size() > 0)
        {
            _audioFrameInputNode.RemoveOutgoingConnection(_audioFrameInputNode.OutgoingConnections().GetAt(0).Destination());
        }
        // TODO: does destruction properly clean this up? _audioFrameInputNode.Dispose();
    }

    void NowSoundTrack::FrameInputNode_QuantumStarted(AudioFrameInputNode sender, FrameInputNodeQuantumStartedEventArgs args)
    {
        Check(sender == _audioFrameInputNode);

        sender.DiscardQueuedFrames();

        DateTime dateTimeNow = DateTime::clock::now();
        TimeSpan sinceLast = dateTimeNow - _lastQuantumTime;
        _lastQuantumTime = dateTimeNow;

        if (IsMuted() || _state != NowSoundTrackState::TrackLooping)
        {
            // copy nothing to anywhere
            return;
        }

        // we are looping; let's play!
        Check(args.RequiredSamples() >= 0);
        uint32_t requiredSamples = (uint32_t)args.RequiredSamples();

        float samplesSinceLastQuantum = ((float)sinceLast.count() * Clock::SampleRateHz / Clock::TicksPerSecond);

        _sinceLastSampleTimingHistogram.Add(samplesSinceLastQuantum);

        // detailed tracing of per-frame timings (for more accurate histogram if desired)
        bool x = false;
        if (x)
        {
            std::wstringstream wstr{};
            wstr.precision(2);
            wstr << L"Samples | " << Clock::Instance().Now().Value()
                << L" | RequiredSamples | " << requiredSamples
                << L" | LocalTime | " << _localTime.Value()
                << L" | SinceLast (samples) | " << samplesSinceLastQuantum;

            DebugLog(wstr.str());
        }

        if (requiredSamples == 0)
        {
            s_zeroByteOutgoingFrameCount++;
            return;
        }

        Windows::Media::AudioFrame audioFrame = NowSoundGraph::Instance()->GetAudioFrame();

        {
            // This nested scope sets the extent of the LockBuffer call below, which must close before the AddFrame call.
            // Otherwise the AddFrame will throw E_ACCESSDENIED when it tries to take a read lock on the frame.
            uint8_t* dataInBytes{};
            uint32_t capacityInBytes{};

            // OMG KENNY KERR WINS AGAIN:
            // https://gist.github.com/kennykerr/f1d941c2d26227abbf762481bcbd84d3
            Windows::Media::AudioBuffer buffer(audioFrame.LockBuffer(Windows::Media::AudioBufferAccessMode::Write));
            IMemoryBufferReference reference(buffer.CreateReference());
            winrt::impl::com_ref<IMemoryBufferByteAccess> interop = reference.as<IMemoryBufferByteAccess>();
            check_hresult(interop->GetBuffer(&dataInBytes, &capacityInBytes));

            // To use low latency pipeline on every quantum, have this use requiredSamples*8 rather than capacityInBytes.
            uint32_t bytesRemaining = capacityInBytes; // requiredSamples * 8;

            int sampleSizeInBytes = MagicNumbers::AudioChannelCount * sizeof(float);

            int samplesRemaining = (int)bytesRemaining / sampleSizeInBytes;

            // TODO: contact Microsoft about this: why is this API taking a timespan rather than an exact sample count?
            // And how does one ensure the timespan conversion is exactly right?
            // For now, avoid this property altogether and tune things with the size of the audio frame;
            // note that *creating* an audio frame takes a sample count and not a timespan duration....
            //audioFrame.Duration(TimeSpan(samplesRemaining * Clock::TicksPerSecond / Clock::SampleRateHz));

            while (samplesRemaining > 0)
            {
                // get up to one second or samplesRemaining, whichever is smaller
                Slice<AudioSample, float> longest(
                    _audioStream.GetSliceContaining(Interval<AudioSample>(_localTime, samplesRemaining)));

                longest.CopyTo(reinterpret_cast<float*>(dataInBytes));

                Time<AudioSample> now(Clock::Instance().Now());

                dataInBytes += longest.SliceDuration().Value() * longest.SliverCount() * sizeof(float);
                _localTime = _localTime + longest.SliceDuration();
                samplesRemaining -= (int)longest.SliceDuration().Value();
            }
        }

        sender.AddFrame(audioFrame);
    }

    // Handle incoming audio data; manage the Recording -> FinishRecording and FinishRecording -> Looping state transitions.
    bool NowSoundTrack::Record(Duration<AudioSample> duration, float* data)
    {
        // TODO: ThreadContract.RequireAudioGraph();

        bool continueRecording = true;
        switch (_state)
        {
        case NowSoundTrackState::TrackRecording:
        {
            // How many complete beats after we record this data?
            Time<AudioSample> durationAsTime((_audioStream.DiscreteDuration() + duration).Value());
            Duration<Beat> completeBeats = Clock::Instance().TimeToCompleteBeats(durationAsTime);

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

            // and actually record the full amount of available data
            _audioStream.Append(duration, data);
            break;
        }

        case NowSoundTrackState::TrackFinishRecording:
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
                _state = NowSoundTrackState::TrackLooping;
                continueRecording = false;

                _audioStream.Append(duration, data);

                // now that we have done our final append, shut the stream at the current duration
                _audioStream.Shut(ExactDuration());
            }
            else
            {
                // capture the full duration
                _audioStream.Append(duration, data);
            }

            break;
        }

        case NowSoundTrackState::TrackLooping:
        {
            Check(false); // Should never still be recording once in looping state
            return false;
        }
        }

        _localTime = Clock::Instance().Now();

        return continueRecording;
    }

    int NowSoundTrack::s_zeroByteOutgoingFrameCount{};
}
