// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include <queue>
#include <string>

#include "stdafx.h"

#include "JuceHeader.h"

#include "SpatialAudioProcessor.h"
#include "Clock.h"
#include "Histogram.h"
#include "NowSoundFrequencyTracker.h"
#include "NowSoundLibTypes.h"
#include "NowSoundTime.h"
#include "Tempo.h"

// set to 1 to reuse a static AudioFrame; 0 will allocate a new AudioFrame in each audio quantum event handler
#define STATIC_AUDIO_FRAME 1

namespace NowSound
{
    // Represents a single looping track of recorded audio.
    // Currently a Track is backed by a mono BufferedSliceStream, but emits stereo output based on current Pan value.
    // This allows Tracks to be panned around even after being recorded. In other words, the stereo balance is not
    // baked into two channels.
    class NowSoundTrackAudioProcessor : public SpatialAudioProcessor
    {
    private:
        // Identifier of this Track.
        const TrackId _trackId;

        // Identifier of the input we record from (for tracking input frequencies while recording).
        const AudioInputId _audioInputId;

        // The current state of the track (recording / finishing / looping).
        NowSoundTrackState _state;

        // The number of complete beats thaat measures the duration of this track.
        // Increases steadily while Recording; sets a time limit to further recording during FinishRecording;
        // remains constant while Looping.
        // TODO: relax this to permit non-quantized looping.
        Duration<Beat> _beatDuration;

        // The stream containing this Track's mono data.
        // This may be shared with copied Tracks.
        std::shared_ptr<BufferedSliceStream<AudioSample, float>> _audioStream;

        // Last sample time is based on the Now when the track started looping, and advances strictly
        // based on what the Track has pushed during looping; this variable should be unused except
        // in Looping state.
        Time<AudioSample> _lastSampleTime;

        // did this just stop recording? if so, message thread will remove its input connection on next poll
        bool _justStoppedRecording;

        // What is the BPM (beats per minute) of this track?
        // Tracks retain the BPM that existed at their creation (at least until we implement track duration/tempo change).
        std::unique_ptr<Tempo> _tempo;

    public: // Non-exported methods for internal use

        // New constructor
        NowSoundTrackAudioProcessor(
            NowSoundGraph* graph,
            TrackId trackId,
            AudioInputId inputId,
            const BufferedSliceStream<AudioSample, float>& sourceStream,
            float initialVolume,
            float initialPan,
            float beatsPerMinute,
            int beatsPerMeasure);

        // Copy constructor; shares same stream. Only supported when other is looping.
        NowSoundTrackAudioProcessor(TrackId trackId, NowSoundTrackAudioProcessor* other);

        // JUCE processing method; this is called on the audio thread and may not make graph changes.
        virtual void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;

        // Did this track stop recording since the last time this method was called?
        // The message thread polls this value to determine when to remove tracks' input connections after recording.
        bool JustStoppedRecording();

        // If we are recording, monitor the input; otherwise, monitor the track itself.
        virtual NowSoundSignalInfo SignalInfo() override;

        // If we are recording, monitor the input; otherwise, monitor the track itself.
        virtual void GetFrequencies(void* floatBuffer, int floatBufferCapacity) override;

    public: // Exported methods via NowSoundTrackAPI

        // In what state is this track?
        NowSoundTrackState State() const;
        
        // What is this track's tempo in beats per minute?
        float BeatsPerMinute() const;

        // What is this track's time signature?
        int BeatsPerMeasure() const;

        // Duration in beats of track's BeatsPerMinute.
        // Note that this is discrete (not fractional). This doesn't yet support non-beat-quantization.
        Duration<Beat> BeatDuration() const;

        // What beat position is playing right now?
        // This uses Clock.Instance.Now to determine the current time, and is continuous because we may be
        // playing a fraction of a beat right now.  It will always be strictly less than BeatDuration.
        ContinuousDuration<Beat> BeatPositionUnityNow() const;

        // How long is this track, in samples?
        // This is increased during recording.  It may in general have fractional numbers of samples if 
        // BeatsPerMinute() does not evenly divide Clock::Instance().SampleRateHz.
        ContinuousDuration<AudioSample> ExactDuration() const;

        // The starting moment at which this Track was created.
        Time<AudioSample> StartTime() const;

        // The full time info for this track (to allow just one call per track for all this info).
        // Note that this is not const because it may recalculate histograms etc. when called.
        NowSoundTrackInfo Info();

        // The user wishes the track to finish recording now.
        // Contractually requires State == NowSoundTrack_State::Recording.
        void FinishRecording();
    };
}
