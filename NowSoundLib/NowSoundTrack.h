// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include <queue>
#include <string>

#include "pch.h"

#include "Clock.h"
#include "Histogram.h"
#include "NowSoundLibTypes.h"
#include "Recorder.h"
#include "Time.h"

// set to 1 to reuse a static AudioFrame; 0 will allocate a new AudioFrame in each audio quantum event handler
#define STATIC_AUDIO_FRAME 1

namespace NowSound
{
    class NowSoundTrack : public IRecorder<AudioSample, float>
    {
    public:
        // non-exported methods for "internal" use
        static void AddTrack(TrackId id, std::unique_ptr<NowSoundTrack>&& track);

        // Accessor for track by ID.
        static NowSoundTrack* Track(TrackId id);

        static void DeleteTrack(TrackId id);

    private:
        // The collection of all ttracks.
        static std::map<TrackId, std::unique_ptr<NowSoundTrack>> s_tracks;

        // How many outgoing frames had zero bytes requested?  (can not understand why this would ever happen)
        static int s_zeroByteOutgoingFrameCount;

#if STATIC_AUDIO_FRAME
        // Audio frame, reused for copying audio.
        static winrt::Windows::Media::AudioFrame s_audioFrame;
#endif

        // Sequence number of this Track; purely diagnostic, never exposed to outside except diagnostically.
        const TrackId _trackId;

        // The input the track is recording from, if recording.
        const AudioInputId _inputId;

        // The current state of the track.
        NowSoundTrackState _state;

        // The number of complete beats thaat measures the duration of this track.
        // Increases steadily while Recording; sets a time limit to further recording during FinishRecording;
        // remains constant while Looping.
        // TODO: relax this to permit non-quantized looping.
        Duration<Beat> _beatDuration;

        // The node this track uses for emitting data into the audio graph.
        // This is the output node for this Track, but the input node for the audio graph.
        winrt::Windows::Media::Audio::AudioFrameInputNode _audioFrameInputNode;

        // The stream containing this Track's data; this is an owning reference.
        BufferedSliceStream<AudioSample, float> _audioStream;

        // Last sample time is based on the Now when the track started looping, and advances strictly
        // based on what the Track has pushed during looping; this variable should be unused except
        // in Looping state.
        Time<AudioSample> _lastSampleTime;

        winrt::Windows::Foundation::DateTime _lastQuantumTime;

        bool _isMuted;

        // for debug logging; need to understand micro-behavior of the frame input node
        std::queue<std::wstring> _debugLog;

        void DebugLog(const std::wstring& entry);

        // histogram of required samples count
        Histogram _requiredSamplesHistogram;

        // histogram of time since last sample request
        Histogram _sinceLastSampleTimingHistogram;

		// histogram of volume
		Histogram _recentVolumeHistogram;

    public:
        NowSoundTrack(TrackId trackId, AudioInputId inputId, const BufferedSliceStream<AudioSample, float>& sourceStream);

        // In what state is this track?
        NowSoundTrackState State() const;

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

        // The full time info for this track (to allow just one call per track for all this info).
        NowSoundTrackInfo Info() const;

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

        // The quantum has started; consume input audio for this recording.
        void FrameInputNode_QuantumStarted(
            winrt::Windows::Media::Audio::AudioFrameInputNode sender,
            winrt::Windows::Media::Audio::FrameInputNodeQuantumStartedEventArgs args);

        // Record from (that is, copy from) the source data.
        virtual bool Record(Duration<AudioSample> duration, float* source);
    };
}