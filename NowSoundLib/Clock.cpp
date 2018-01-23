// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "BufferAllocator.h"

using namespace NowSound;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace Windows::Foundation;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Render;

{
	// 
	// Tracks the current time (driven from the audio input if it exists, otherwise from the Unity deltaTime),
	// and  converts it to seconds and beats.
	// 
	// 
	// Since the audio thread is fundamentally driving the time if it exists, the current clock
	// reading is subject to change out from under the UI thread.  So the Clock hands out immutable
	// Moment instances, which represent the time at the moment the clock was asked.  Moments in
	// turn can be converted to timepoint-counts,  seconds, and beats, consistently and without racing.
	// </remarks>
	public class Clock
{
	private static Clock s_instance = null;

	// 
	// The singleton Clock used by the application.
	// 
	// 
	// The clock must have been constructed before calling this method.
	// </remarks>
	public static Clock Instance
	{
		get
	{
		Contract.Requires(s_instance != null, "clock must have been initialized");
	return s_instance;
	}
	}

		// 
		// Get the clock even if it has not been initialized yet.
		// 
		public static Clock PossiblyNullInstance
	{
		get
	{
		return s_instance;
	}
	}

		// The rate of sound measurements (individual sample data points) per second.
	public const int SampleRateHz = 48000;

	// The current BPM of this Clock.
	float _beatsPerMinute;

	// The beats per MEASURE.  e.g. 3/4 time = 3 beats per measure.
	// TODO: make this actually mean something; it is only partly implemented right now.
	readonly int _beatsPerMeasure;

	// 
	// The number of samples since the beginning of Holofunk; incremented by the audio quantum.
	// 
	Time<AudioSample> _audioTime;

	// 
	// The number of samples since the beginning of Holofunk; incremented (to the current value of AudioNow)
	// at the start of each Unity Update() cycle.
	// 
	Time<AudioSample> _unityTime;

	// 
	// How many input channels are there?
	// 
	readonly int _inputChannelCount;

	// What is the floating-point duration of a beat, in samples?
	// This will be a non-integer value if the BPM does not exactly divide the sample rate.
	ContinuousDuration<AudioSample> _beatDuration;

	// 
	// Is this clock advanced by the Unity thread pool?
	// 
	// 
	// To avoid mistaken updates, the clock can only be advanced from one thread pool.
	// </remarks>        
	Option<bool> _isAdvancedByUnity = default(Option<bool>);

	const double TicksPerSecond = 10 * 1000 * 1000;

	// 
	// Construct a Clock and initialize Clock.Instance.
	// 
	// 
	// This must be called exactly once per process; multiple calls will be contract failure (unless closely
	// overlapped in time in which case they will race).
	// </remarks>
	public Clock(float beatsPerMinute, int beatsPerMeasure, int inputChannelCount)
	{
		Contract.Requires(s_instance == null, "No Clock yet");

		_beatsPerMinute = beatsPerMinute;
		_beatsPerMeasure = beatsPerMeasure;
		_inputChannelCount = inputChannelCount;

		CalculateBeatDuration();

		// and initialize.
		s_instance = this;
	}

	void CalculateBeatDuration()
	{
		_beatDuration = (ContinuousDuration<AudioSample>)(((float)SampleRateHz * 60f) / _beatsPerMinute);
	}

	// Advance this clock from a Unity thread.
	public void AdvanceFromUnity(Duration<AudioSample> duration)
	{
		ThreadContract.RequireUnity();

		if (!_isAdvancedByUnity.HasValue)
		{
			_isAdvancedByUnity = true;
		}
		else
		{
			Contract.Requires(_isAdvancedByUnity.Value, "Clock must not be advanced by both audio graph thread and Unity thread");
		}

		_audioTime += duration;
	}

	// Advance this clock from an AudioGraph thread.
	public void AdvanceFromAudioGraph(Duration<AudioSample> duration)
	{
		ThreadContract.RequireAudioGraph();

		if (!_isAdvancedByUnity.HasValue)
		{
			_isAdvancedByUnity = false;
		}
		else
		{
			Contract.Requires(!_isAdvancedByUnity.Value, "Clock must not be advanced by both audio graph thread and Unity thread");
		}

		_audioTime += duration;
	}

	// 
	// Advance UnityNow to the current value of AudioNow.
	// 
	public void SynchronizeUnityTimeWithAudioTime()
	{
		_unityTime = _audioTime;
	}

	// The beats per minute of this clock.
	// This is the most useful value for humans to control and see, and in fact pretty much all 
	// time in the system is derived from this.  This value can only currently be changed when
	// no tracks exist.</remarks>
	public float BPM
	{
		get
	{
		return _beatsPerMinute;
	}
		set
	{
		_beatsPerMinute = value;
	CalculateBeatDuration();
	}
	}

		// 
		// The current audio time; advanced by the audio quantum.
		// 
		public Moment AudioNow
	{
		// Stereo channel, hence twice as many samples as timepoints.
		get
	{
		return TimeToMoment(_audioTime);
	}
	}

		public int BytesPerSecond
	{
		get
	{
		return SampleRateHz * _inputChannelCount * 4; // float samples x channels x sample rate
	}
	}

		// 
		// The value of AudioNow as of the start of the last Unity quantum.
		// 
		// 
		// All Unity code should use this value of Now, as it will not advance during a Unity Update() cycle.
		// </remarks>
		public Moment UnityNow
	{
		get
	{
		return TimeToMoment(_unityTime);
	}
	}

		public Moment TimeToMoment(Time<AudioSample> time)
	{
		return new Moment(time, this);
	}

	public Moment DurationToMoment(Duration<AudioSample> duration)
	{
		return new Moment((int)duration, this);
	}

	public double BeatsPerSecond
	{
		get{ return ((double)_beatsPerMinute) / 60.0; }
	}

		public ContinuousDuration<AudioSample> BeatDuration
	{
		get{ return _beatDuration; }
	}

		public int BeatsPerMeasure
	{
		get{ return _beatsPerMeasure; }
	}
}

// Moments are immutable points in time, that can be converted to various
// time measurements (timepoint-count, second, beat).
public struct Moment
{
	public readonly Time<AudioSample> Time;

	public readonly Clock Clock;

	internal Moment(Time<AudioSample> time, Clock clock)
	{
		Time = time;
		Clock = clock;
	}

	// Approximately how many seconds?
	public double Seconds{ get{ return ((double)Time) / Clock.SampleRateHz; } }

		// Exactly how many beats?
	public ContinuousDuration<Beat> Beats{ get{ return (ContinuousDuration<Beat>)((double)Time / (double)Clock.BeatDuration); } }

		// Exactly how many complete beats?
		// Beats are represented by ints as it's hard to justify longs; 2G beats = VERY LONG TRACK</remarks>
	public Duration<Beat> CompleteBeats{ get{ return (int)Beats; } }

	private const double Epsilon = 0.0001; // empirically seen some Beats values come too close to this

										   // What fraction of a beat?
	public ContinuousDuration<Beat> FractionalBeat{ get{ return (ContinuousDuration<Beat>)((float)Beats - (float)CompleteBeats); } }

		public override string ToString()
	{
		return "Moment[" + Time + "]";
	}
}
}
