// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include <queue>
#include <string>

#include "stdafx.h"

#include "JuceHeader.h"

#include "BaseAudioProcessor.h"
#include "Clock.h"
#include "Histogram.h"
#include "NowSoundFrequencyTracker.h"
#include "NowSoundLibTypes.h"
#include "Recorder.h"
#include "NowSoundTime.h"

// set to 1 to reuse a static AudioFrame; 0 will allocate a new AudioFrame in each audio quantum event handler
#define STATIC_AUDIO_FRAME 1

namespace NowSound
{
	// Represents a single looping track of recorded audio.
	// Currently a Track is backed by a mono BufferedSliceStream, but emits stereo output based on current Pan value.
    class NowSoundTrackAudioProcessor : public BaseAudioProcessor
    {
    public:
        // non-exported methods for "internal" use
        static void AddTrack(TrackId id, juce::AudioProcessorGraph::Node::Ptr track);

        // Accessor for track by ID.
        static NowSoundTrackAudioProcessor* Track(TrackId id);

        static void DeleteTrack(TrackId id);

    private:
        // The collection of all ttracks.
        static std::map<TrackId, juce::AudioProcessorGraph::Node::Ptr> s_tracks;

#if STATIC_AUDIO_FRAME
        // Audio frame, reused for copying audio.
        // static winrt::Windows::Media::AudioFrame s_audioFrame;
#endif

		// The graph that created this.
		const NowSoundGraph* _graph;

        // Sequence number of this Track; purely diagnostic, never exposed to outside except diagnostically.
        const TrackId _trackId;

        // The input the track is recording from, if recording.
        const AudioInputId _inputId;

		// The frequency tracker for this track.
		const std::unique_ptr<NowSoundFrequencyTracker> _frequencyTracker;

        // The current state of the track.
        NowSoundTrackState _state;

        // The number of complete beats thaat measures the duration of this track.
        // Increases steadily while Recording; sets a time limit to further recording during FinishRecording;
        // remains constant while Looping.
        // TODO: relax this to permit non-quantized looping.
        Duration<Beat> _beatDuration;

        // The stream containing this Track's data; this is an owning reference.
        BufferedSliceStream<AudioSample, float> _audioStream;

        // Last sample time is based on the Now when the track started looping, and advances strictly
        // based on what the Track has pushed during looping; this variable should be unused except
        // in Looping state.
        Time<AudioSample> _lastSampleTime;

        bool _isMuted;

        // for debug logging; need to understand micro-behavior of the frame input node
        std::queue<std::wstring> _debugLog;

        void DebugLog(const std::wstring& entry);

        // histogram of required samples count
        Histogram _requiredSamplesHistogram;

        // histogram of time since last sample request
        Histogram _sinceLastSampleTimingHistogram;

		// histogram of volume
		Histogram _volumeHistogram;

		// current pan value; 0 = left, 0.5 = center, 1 = right
		float _pan;

        // did this just stop recording? if so, message thread will remove its input connection on next poll
        bool _justStoppedRecording;

    public: // Non-exported methods for internal use
		NowSoundTrackAudioProcessor(
			NowSoundGraph* graph,
			TrackId trackId,
			AudioInputId inputId,
			const BufferedSliceStream<AudioSample, float>& sourceStream,
			float initialPan);

        virtual const String getName() const { return L"NowSoundTrackAudioProcessor"; }

        virtual void processBlock(
            AudioBuffer<float>& buffer,
            MidiBuffer& midiMessages) override;

        // Did this track stop recording since the last time this method was called?
        // The message thread polls this value to determine when to remove tracks' input connections after recording.
        bool JustStoppedRecording();

    public: // Exported methods via NowSoundTrackAPI

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
		// Note that this is not const because it may recalculate histograms etc. when called.
        NowSoundTrackInfo Info();

        // The user wishes the track to finish recording now.
        // Contractually requires State == NowSoundTrack_State::Recording.
        void FinishRecording();

		// Get the frequency histogram, by updating the given WCHAR buffer as though it were a float* buffer.
		void GetFrequencies(void* floatBuffer, int floatBufferCapacity);

		// True if this is muted.
        // 
        // Note that something can be in FinishRecording state but still be muted, if the user is fast!
        // Hence this is a separate flag, not represented as a NowSoundTrack_State.
        bool IsMuted() const;
        void SetIsMuted(bool isMuted);

		// Get and set the pan value for this track.
		float Pan();
		void Pan(float pan);

        // Delete this Track; after this, all methods become invalid to call (contract failure).
        void Delete();
    };
}
