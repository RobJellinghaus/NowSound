// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

#include "Clock.h"
#include "NowSoundLibTypes.h"
#include "Recorder.h"
#include "Time.h"

using namespace NowSound;
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
    class NowSoundTrack : IRecorder<AudioSample, float>
    {
        // How many outgoing frames had zero bytes requested?  (can not understand why this would ever happen)
        static int s_zeroByteOutgoingFrameCount;

        // Sequence number of this Track; purely diagnostic, never exposed to outside except diagnostically.
        const TrackId _trackId;

        // The audio graph which created this Track.
        // HolofunkAudioGraph _audioGraph; // TODO: do we want to have graph objects? Might we want multiple graphs for different output devices?

        // The input the track is recording from, if recording.
        const AudioInputId _inputId;

        // The current state of the track.
        NowSoundTrack_State _state;

        // The number of complete beats thaat measures the duration of this track.
        // Increases steadily while Recording; sets a time limit to further recording during FinishRecording;
        // remains constant while Looping.
        // TODO: relax this to permit non-quantized looping.
        Duration<Beat> _beatDuration;

        // The starting time of this track.
        const Time<AudioSample> _startTime;

        // The node this track uses for emitting data into the audio graph.
        // This is the output node for this Track, but the input node for the audio graph.
        Windows::Media::Audio::AudioFrameInputNode _audioFrameInputNode;

        // The stream containing this Track's data; this is an owning reference.
        BufferedSliceStream<AudioSample, float> _audioStream;

        // Local time is based on the Now when the track started looping, and advances strictly
        // based on what the Track has pushed during looping; this variable should be unused except
        // in Looping state.
        // 
        // HISTORICAL EXPLANATION FROM ASIO ERA:
        // 
        // This is because BASS doesn't have rock-solid consistency between the MixerToOutputAsioProc
        // and the Track's SyncProc.  We would expect that if at time T we push N samples from this
        // track, that the SyncProc would then be called back at exactly time T+N.  However, this is not
        // in fact the case -- there is some nontrivial variability of +/- one sample buffer.  So we
        // use a local time to avoid requiring this exact timing behavior from BASS.
        // 
        // The previous code just pushed one sample buffer after another based on their indices; it paid
        // no attention to "global time" at all.
        // 
        // CURRENT NOTE FROM AUDIOGRAPH ERA:
        // 
        // It will be interesting to see whether this complexity is still warranted, but since it seems
        // a reasonably robust strategy, we will stick with it at this point; future testing possible if
        // energy is available.
        Time<AudioSample> _localTime;

        DateTime _lastQuantumTime;

        bool _isMuted;

    public:
        NowSoundTrack(TrackId trackId, AudioInputId inputId);

        // In what state is this track?
        NowSoundTrack_State State() const;

        // Duration in beats of current Clock.
        // Note that this is discrete (not fractional). This doesn't yet support non-beat-quantization.
        Duration<Beat> BeatDuration() const;

        // What beat position is playing right now?
        // This uses Clock.Instance.Now to determine the current time, and is continuous because we may be
        // playing a fraction of a beat right now.  It will always be strictly less than BeatDuration.
        ContinuousDuration<Beat> BeatPositionUnityNow() const;

        // How long is this track, in samples?
        // This is increased during recording.  It may in general have fractional numbers of samples if 
        // Clock::Instance().BeatsPerMinute does not evenly divide Clock::Instance().SampleRateHz.
        ContinuousDuration<AudioSample> ExactDuration() const;

        // The starting moment at which this Track was created.
        Time<AudioSample> StartTime() const;

        // The user wishes the track to finish recording now.
        // Contractually requires State == NowSoundTrack_State::Recording.
        void FinishRecording();

        // True if this is muted.
        // 
        // Note that something can be in FinishRecording state but still be muted, if the user is fast!
        // Hence this is a separate flag, not represented as a NowSoundTrack_State.
        bool IsMuted() const;
        void SetIsMuted(bool isMuted);

        // Delete this Track; after this, all methods become invalid to call (contract failure).
        void Delete();

        // TODO: Hack? Update the track to increment, e.g., its duration. (Should perhaps instead be computed whenever BeatDuration is queried???)
        void UnityUpdate();

        // The quantum has started; consume input audio for this recording.
        void FrameInputNode_QuantumStarted(AudioFrameInputNode sender, FrameInputNodeQuantumStartedEventArgs args);

        // Record from (that is, copy from) the source data.
        virtual bool Record(Time<AudioSample> time, Duration<AudioSample> duration, float* source);
    };
}