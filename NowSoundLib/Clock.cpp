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
	/// <summary>
	/// Tracks the current time (driven from the audio input if it exists, otherwise from the Unity deltaTime),
	/// and  converts it to seconds and beats.
	/// </summary>
	/// <remarks>
	/// Since the audio thread is fundamentally driving the time if it exists, the current clock
	/// reading is subject to change out from under the UI thread.  So the Clock hands out immutable
	/// Moment instances, which represent the time at the moment the clock was asked.  Moments in
	/// turn can be converted to timepoint-counts,  seconds, and beats, consistently and without racing.
	/// </remarks>
	public class Clock
{
	private static Clock s_instance = null;

	/// <summary>
	/// The singleton Clock used by the application.
	/// </summary>
	/// <remarks>
	/// The clock must have been constructed before calling this method.
	/// </remarks>
	public static Clock Instance
	{
		get
	{
		Contract.Requires(s_instance != null, "clock must have been initialized");
	return s_instance;
	}
	}

		/// <summary>
		/// Get the clock even if it has not been initialized yet.
		/// </summary>
		public static Clock PossiblyNullInstance
	{
		get
	{
		return s_instance;
	}
	}

		/// <summary>The rate of sound measurements (individual sample data points) per second.</summary>
	public const int SampleRateHz = 48000;

	// The current BPM of this Clock.
	float _beatsPerMinute;

	// The beats per MEASURE.  e.g. 3/4 time = 3 beats per measure.
	// TODO: make this actually mean something; it is only partly implemented right now.
	readonly int _beatsPerMeasure;

	/// <summary>
	/// The number of samples since the beginning of Holofunk; incremented by the audio quantum.
	/// </summary>
	Time<AudioSample> _audioTime;

	/// <summary>
	/// The number of samples since the beginning of Holofunk; incremented (to the current value of AudioNow)
	/// at the start of each Unity Update() cycle.
	/// </summary>
	Time<AudioSample> _unityTime;

	/// <summary>
	/// How many input channels are there?
	/// </summary>
	readonly int _inputChannelCount;

	/// <summary>What is the floating-point duration of a beat, in samples?</summary>
	// This will be a non-integer value if the BPM does not exactly divide the sample rate.
	ContinuousDuration<AudioSample> _beatDuration;

	/// <summary>
	/// Is this clock advanced by the Unity thread pool?
	/// </summary>
	/// <remarks>
	/// To avoid mistaken updates, the clock can only be advanced from one thread pool.
	/// </remarks>        
	Option<bool> _isAdvancedByUnity = default(Option<bool>);

	const double TicksPerSecond = 10 * 1000 * 1000;

	/// <summary>
	/// Construct a Clock and initialize Clock.Instance.
	/// </summary>
	/// <remarks>
	/// This must be called exactly once per process; multiple calls will be contract failure (unless closely
	/// overlapped in time in which case they will race).
	/// </remarks>
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

	/// <summary>Advance this clock from a Unity thread.</summary>
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

	/// <summary>Advance this clock from an AudioGraph thread.</summary>
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

	/// <summary>
	/// Advance UnityNow to the current value of AudioNow.
	/// </summary>
	public void SynchronizeUnityTimeWithAudioTime()
	{
		_unityTime = _audioTime;
	}

	/// <summary>The beats per minute of this clock.</summary>
	/// <remarks>This is the most useful value for humans to control and see, and in fact pretty much all 
	/// time in the system is derived from this.  This value can only currently be changed when
	/// no tracks exist.</remarks>
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

		/// <summary>
		/// The current audio time; advanced by the audio quantum.
		/// </summary>
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

		/// <summary>
		/// The value of AudioNow as of the start of the last Unity quantum.
		/// </summary>
		/// <remarks>
		/// All Unity code should use this value of Now, as it will not advance during a Unity Update() cycle.
		/// </remarks>
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

/// <summary>Moments are immutable points in time, that can be converted to various
/// time measurements (timepoint-count, second, beat).</summary>
public struct Moment
{
	public readonly Time<AudioSample> Time;

	public readonly Clock Clock;

	internal Moment(Time<AudioSample> time, Clock clock)
	{
		Time = time;
		Clock = clock;
	}

	/// <summary>Approximately how many seconds?</summary>
	public double Seconds{ get{ return ((double)Time) / Clock.SampleRateHz; } }

		/// <summary>Exactly how many beats?</summary>
	public ContinuousDuration<Beat> Beats{ get{ return (ContinuousDuration<Beat>)((double)Time / (double)Clock.BeatDuration); } }

		/// <summary>Exactly how many complete beats?</summary>
		/// <remarks>Beats are represented by ints as it's hard to justify longs; 2G beats = VERY LONG TRACK</remarks>
	public Duration<Beat> CompleteBeats{ get{ return (int)Beats; } }

	private const double Epsilon = 0.0001; // empirically seen some Beats values come too close to this

										   /// <summary>What fraction of a beat?</summary>
	public ContinuousDuration<Beat> FractionalBeat{ get{ return (ContinuousDuration<Beat>)((float)Beats - (float)CompleteBeats); } }

		public override string ToString()
	{
		return "Moment[" + Time + "]";
	}
}
}
