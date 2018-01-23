#pragma once

// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

namespace NowSound
{
	/// <summary>
	/// Sample identifies Times based on audio sample counts. 
	/// </summary>
	/// <remarks>
	/// This type is actually never instantiated; it is used purely as a generic type parameter.
	/// </remarks>
	public class AudioSample
{
}

/// <summary>
/// Beat identifies times based on beat counts.
/// </summary>
/// <remarks>
/// This type is actually never instantiated; it is used purely as a generic type parameter.
/// </remarks>
public class Beat
{
}

/// <summary>
/// Seconds identifies times based on real-world seconds.
/// </summary>
/// <remarks>
/// This type is actually never instantiated; it is used purely as a generic type parameter.
/// </remarks>
public class Second
{
}

/// <summary>
/// Frame identifies Times based on video frame counts. 
/// </summary>
/// <remarks>
/// This type is actually never instantiated; it is used purely as a generic type parameter.
/// </remarks>
public class Frame
{
}

/// <summary>
/// Time parameterized on some underlying measurement.
/// </summary>
public struct Time<TTime>
{
	readonly long m_time;

	public Time(long time)
	{
		m_time = time;
	}

	public override string ToString()
	{
		return "T[" + (long)this + "]";
	}

	public static Time<TTime> Min(Time<TTime> first, Time<TTime> second)
	{
		return new Time<TTime>(Math.Min(first, second));
	}

	public static Time<TTime> Max(Time<TTime> first, Time<TTime> second)
	{
		return new Time<TTime>(Math.Max(first, second));
	}

	public static implicit operator long(Time<TTime> time)
	{
		return time.m_time;
	}

	public static implicit operator Time<TTime>(long time)
	{
		return new Time<TTime>(time);
	}

	public static explicit operator Time<TTime>(Duration<TTime> duration)
	{
		return new Time<TTime>(duration);
	}

	public static bool operator <(Time<TTime> first, Time<TTime> second)
	{
		return (long)first < (long)second;
	}

	public static bool operator >(Time<TTime> first, Time<TTime> second)
	{
		return (long)first > (long)second;
	}

	public static bool operator ==(Time<TTime> first, Time<TTime> second)
	{
		return (long)first == (long)second;
	}

	public static bool operator !=(Time<TTime> first, Time<TTime> second)
	{
		return (long)first != (long)second;
	}

	public static bool operator <=(Time<TTime> first, Time<TTime> second)
	{
		return (long)first <= (long)second;
	}

	public static bool operator >=(Time<TTime> first, Time<TTime> second)
	{
		return (long)first >= (long)second;
	}

	public static Duration<TTime> operator -(Time<TTime> first, Time<TTime> second)
	{
		return new Duration<TTime>((long)first - (long)second);
	}

	public static Time<TTime> operator -(Time<TTime> first, Duration<TTime> second)
	{
		return new Time<TTime>((long)first - (long)second);
	}

	public override bool Equals(object obj)
	{
		return obj is Time<TTime> && ((Time<TTime>)obj).m_time == m_time;
	}

	public override int GetHashCode()
	{
		return m_time.GetHashCode();
	}
}

/// <summary>
/// A distance between two Times.
/// </summary>
/// <typeparam name="TTime"></typeparam>
public struct Duration<TTime>
{
	readonly long m_count;

	public Duration(long count)
	{
		Contract.Requires(count >= 0, "count >= 0");
		m_count = count;
	}

	public override string ToString()
	{
		return "D[" + (long)this + "]";
	}

	public static implicit operator long(Duration<TTime> offset)
	{
		return offset.m_count;
	}

	public static implicit operator Duration<TTime>(long value)
	{
		return new Duration<TTime>(value);
	}

	public static explicit operator Duration<TTime>(Time<TTime> value)
	{
		return new Duration<TTime>(value);
	}

	public static Duration<TTime> Min(Duration<TTime> first, Duration<TTime> second)
	{
		return new Duration<TTime>(Math.Min(first, second));
	}

	public static Duration<TTime> operator +(Duration<TTime> first, Duration<TTime> second)
	{
		return new Duration<TTime>((long)first + (long)second);
	}

	public static Duration<TTime> operator -(Duration<TTime> first, Duration<TTime> second)
	{
		return new Duration<TTime>((long)first - (long)second);
	}

	public static Duration<TTime> operator /(Duration<TTime> first, int second)
	{
		return new Duration<TTime>((long)first / second);
	}

	public static bool operator <(Duration<TTime> first, Duration<TTime> second)
	{
		return (long)first < (long)second;
	}

	public static bool operator >(Duration<TTime> first, Duration<TTime> second)
	{
		return (long)first > (long)second;
	}

	public static bool operator <=(Duration<TTime> first, Duration<TTime> second)
	{
		return (long)first <= (long)second;
	}

	public static bool operator >=(Duration<TTime> first, Duration<TTime> second)
	{
		return (long)first >= (long)second;
	}

	public static bool operator ==(Duration<TTime> first, Duration<TTime> second)
	{
		return (long)first == (long)second;
	}

	public static bool operator !=(Duration<TTime> first, Duration<TTime> second)
	{
		return (long)first != (long)second;
	}

	public static Time<TTime> operator +(Time<TTime> first, Duration<TTime> second)
	{
		return new Time<TTime>((long)first + (long)second);
	}

	public override bool Equals(object obj)
	{
		return obj is Duration<TTime> && ((Duration<TTime>)obj).m_count == m_count;
	}

	public override int GetHashCode()
	{
		return m_count.GetHashCode();
	}
}

/// <summary>
/// An interval, defined as a start time and a duration (aka length).
/// </summary>
/// <remarks>
/// Empty intervals semantically have no InitialTime, and no distinction should be made between empty
/// intervals based on InitialTime.</remarks>
/// <typeparam name="TTime"></typeparam>
public struct Interval<TTime>
{
	public readonly Time<TTime> InitialTime;
	public readonly Duration<TTime> Duration;
	readonly bool m_isInitialized;

	public Interval(Time<TTime> initialTime, Duration<TTime> duration)
	{
		Contract.Requires(duration >= 0, "duration >= 0");

		InitialTime = initialTime;
		Duration = duration;
		m_isInitialized = true;
	}

	public override string ToString()
	{
		return "I[" + InitialTime + ", " + Duration + "]";
	}

	public static Interval<TTime> Empty{ get{ return new Interval<TTime>(0, 0); } }

	public bool IsInitialized{ get{ return m_isInitialized; } }

		public bool IsEmpty
	{
		get{ return Duration == 0; }
	}

		public Interval<TTime> SubintervalStartingAt(Duration<TTime> offset)
	{
		Debug.Assert(offset <= Duration);
		return new Interval<TTime>(InitialTime + offset, Duration - offset);
	}

	public Interval<TTime> SubintervalOfDuration(Duration<TTime> duration)
	{
		Debug.Assert(duration <= Duration);
		return new Interval<TTime>(InitialTime, duration);
	}

	public Interval<TTime> Intersect(Interval<TTime> other)
	{
		Time<TTime> intersectionStart = Time<TTime>.Max(InitialTime, other.InitialTime);
		Time<TTime> intersectionEnd = Time<TTime>.Min(InitialTime + Duration, other.InitialTime + other.Duration);

		if (intersectionEnd < intersectionStart) {
			return Interval<TTime>.Empty;
		}
		else {
			return new Interval<TTime>(intersectionStart, intersectionEnd - intersectionStart);
		}
	}

	public bool Contains(Time<TTime> time)
	{
		if (IsEmpty) {
			return false;
		}

		return InitialTime <= time
			&& (InitialTime + Duration) > time;
	}

	public override bool Equals(object obj)
	{
		return obj is Interval<TTime> && ((Interval<TTime>)obj).InitialTime == InitialTime && ((Interval<TTime>)obj).Duration == Duration;
	}

	public override int GetHashCode()
	{
		// TODO: XOR is a bad combiner, but better than disregarding both fields. Does Unity have HashUtilities or etc.?
		return InitialTime.GetHashCode() ^ Duration.GetHashCode();
	}
}
}

/// Copyright 2011-2017 by Rob Jellinghaus.  All rights reserved.

namespace Holofunk.Core
{
	/// <summary>
	/// A continous distance between two Times.
	/// </summary>
	/// <typeparam name="TTime"></typeparam>
	public struct ContinuousDuration<TTime>
{
	readonly float m_duration;

	public ContinuousDuration(float duration)
	{
		m_duration = duration;
	}

	public static explicit operator float(ContinuousDuration<TTime> duration)
	{
		return duration.m_duration;
	}

	public static explicit operator ContinuousDuration<TTime>(float value)
	{
		return new ContinuousDuration<TTime>(value);
	}

	public static ContinuousDuration<TTime> operator *(ContinuousDuration<TTime> duration, float value)
	{
		return new ContinuousDuration<TTime>(value * duration.m_duration);
	}
	public static ContinuousDuration<TTime> operator *(float value, ContinuousDuration<TTime> duration)
	{
		return new ContinuousDuration<TTime>(value * duration.m_duration);
	}
}
}
