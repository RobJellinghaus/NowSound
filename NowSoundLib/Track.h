// Copyright 201#pragma once

// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

namespace NowSound
{
    /*
    internal class HolofunkAudioTrack : IHolofunkAudioTrack, Recorder<AudioSample, float>
{
    #region Static state

        // Sequence number of next Track; purely diagnostic, never exposed to outside.
        private static int s_sequenceNumber;

    // 
    // We keep a one-quarter-second (stereo float) AudioFrame and reuse it (between all inputs?! TODO fix this for multiple inputs)
    // 
    // 
    // This should probably be at least one second, but the currently hacked muting implementation simply stops populating output
    // buffers, which therefore still have time to drain.
    // TODO: restructure to use submixer and set output volume on submixer when muting/unmuting, to avoid this issue and allow more efficient bigger buffers here.
    // </remarks>
    static AudioFrame s_audioFrame = new Windows.Media.AudioFrame(Clock.SampleRateHz / 4 * sizeof(float) * 2);

    // 
    // How many outgoing frames had zero bytes requested?  (can not understand why this would ever happen)
    // 
    private int s_zeroByteOutgoingFrameCount;

    #endregion

        // 
        // Sequence number of this Track; purely diagnostic, never exposed to outside except diagnostically.
        // 
        private readonly int _sequenceNumber;

    // 
    // The audio graph which created this Track.
    // 
    private readonly HolofunkAudioGraph _audioGraph;

    // 
    // The current state of the track.
    // 
    private TrackState _state;

    // 
    // The number of complete beats thaat measures the duration of this track.
    // 
    // 
    // Increases steadily while Recording; sets a time limit to further recording during FinishRecording;
    // remains constant while Looping.
    // 
    // TODO: relax this to permit non-quantized looping.
    // </remarks>
    private Duration<Beat> _beatDuration;

    // 
    // The starting moment of recording this track.
    // 
    private readonly Moment _startMoment;

    // 
    // The node this track uses for emitting data into the audio graph.
    // 
    // 
    // This is the output node for this Track, but the input node for the audio graph.
    // </remarks>
    private AudioFrameInputNode _audioFrameInputNode;

    // 
    // The stream containing this Track's data.
    // 
    private DenseSampleFloatStream _audioStream;

    // 
    // Local time is based on the Now when the track started looping, and advances strictly
    // based on what the Track has pushed during looping; this variable should be unused except
    // in Looping state.
    // 
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
    // </remarks>
    Time<AudioSample> _localTime;

    DateTime _lastQuantumTime;

    internal HolofunkAudioTrack(HolofunkAudioGraph audioGraph, AudioInputId inputId)
    {
        // Tracks should only be created from the Unity thread.
        ThreadContract.RequireUnity();

        Contract.Requires(audioGraph != null);
        Contract.Requires(Clock.Instance != null);

        // yes, not thread safe -- but we already assume only one unity thread
        _sequenceNumber = s_sequenceNumber++;

        _audioGraph = audioGraph;
        _state = TrackState.Recording;
        _startMoment = Clock.Instance.UnityNow;

        // Now is when we create the AudioFrameInputNode, because now is when we are sure we are not on the
        // audio thread.
        _audioFrameInputNode = _audioGraph.AudioGraph.CreateFrameInputNode();
        _audioFrameInputNode.AddOutgoingConnection(_audioGraph.AudioDeviceOutputNode);
        _audioFrameInputNode.QuantumStarted += FrameInputNode_QuantumStarted;

        _audioStream = new DenseSampleFloatStream(
            _startMoment.Time,
            audioGraph.AudioAllocator,
            2); // stereo inputs, for now

                // and always start at one beat
        _beatDuration = 1;
    }

    #region Properties

        // 
        // In what state is this track?
        // 
        public TrackState State = > _state;

    // 
    // Duration in beats of current Clock.
    // 
    // 
    // Note that this is discrete (not fractional). This doesn't yet support non-beat-quantization.
    // </remarks>
    public Duration<Beat> BeatDuration = > _beatDuration;

    // 
    // What beat position is playing right now?
    // 
    // 
    // This uses Clock.Instance.UnityNow to determine the current time, and is continuous because we may be
    // playing a fraction of a beat right now.
    // </remarks>
    public ContinuousDuration<Beat> BeatPositionUnityNow
    {
        get
    {
        ThreadContract.RequireUnity();

    Duration<AudioSample> sinceStart = Clock.Instance.UnityNow.Time - _startMoment.Time;
    Moment sinceStartMoment = Clock.Instance.DurationToMoment(sinceStart);

    int completeBeatsSinceStart = (int)sinceStartMoment.CompleteBeats % (int)BeatDuration;
    return (ContinuousDuration<Beat>)(completeBeatsSinceStart + (float)sinceStartMoment.FractionalBeat);
    }
    }

        // 
        // How long is this track?
        // 
        // 
        // This is increased during recording.  It may in general have fractional numbers of samples.
        // </remarks>
    public ContinuousDuration<AudioSample> Duration = > (int)BeatDuration * Clock.Instance.BeatDuration;

    // 
    // The starting moment at which this Track was created.
    // 
    public Moment StartMoment = > _startMoment;

    // 
    // True if this is muted.
    // 
    // 
    // Note that something can be in FinishRecording state but still be muted, if the user is fast!
    // Hence this is a separate flag, not represented as a TrackState.
    // </remarks>
    public bool IsMuted{ get; set; }

    public DenseSampleFloatStream Stream = > throw new NotImplementedException();

    #endregion

        // 
        // The user wishes the track to finish recording now.
        // 
        // 
        // Contractually requires State == TrackState.Recording.
        // </remarks>
        public void FinishRecording()
    {
        ThreadContract.RequireUnity();

        // no need for any synchronization at all; the Record() logic will see this change
        _state = TrackState.FinishRecording;
    }

    // 
    // Delete this Track; after this, all methods become invalid to call (contract failure).
    // 
    public void Delete()
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
    public void UnityUpdate()
    {
        ThreadContract.RequireUnity();
    }

    // 
    // Handle a frame of outgoing audio to the output device.
    // 
    // 
    // There is a very interesting tradeoff here.  It is possible to push more audio into the FrameInputNode
    // than is requested; requiredSamples is a minimum value, but more data can be provided.  Below, the 
    // definition of bytesRemaining allows you to choose either alternative, or any value in between,
    // based on whether you want minimum callbacks to the app (return a full buffer of data), or low-latency
    // app responsiveness (return only the minimum requiredSamples).
    // </remarks>
    private unsafe void FrameInputNode_QuantumStarted(AudioFrameInputNode sender, FrameInputNodeQuantumStartedEventArgs args)
    {
        ThreadContract.RequireAudioGraph();

        DateTime dateTimeNow = DateTime.Now;

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
    public bool Record(Moment now, Duration<AudioSample> duration, IntPtr data)
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
                    Contract.Assert(completeBeats < BeatDuration, "completeBeats < BeatDuration");
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
                Contract.Assert(_audioStream.DiscreteDuration <= roundedUpDuration, "_localTime <= roundedUpDuration");

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
}
*/
}
