#pragma once

// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "math.h"

using namespace winrt;

namespace NowSound
{
    // Sample identifies Times based on audio sample counts. 
    // This type is actually never instantiated; it is used purely as a generic type parameter.
    class AudioSample {};

    // Beat identifies times based on beat counts.
    // This type is actually never instantiated; it is used purely as a generic type parameter.
    class Beat {};

    // Seconds identifies times based on real-world seconds.
    // This type is actually never instantiated; it is used purely as a generic type parameter.
    class Second {};

    // Frame identifies Times based on video frame counts. 
    // This type is actually never instantiated; it is used purely as a generic type parameter.
    class Frame {};

    // Time parameterized on some underlying unit of measurement (such as those above).
    // This theoretically could also provide a timing base (e.g. number of units per other-unit),
    // but currently does not.
    template<typename TUnit>
    struct Time
    {
        // The number of TTime units represented by this time.
        const long _time;

        Time(long time)
        {
            _time = time;
        }

        static Time<TTime> Min(const Time<TTime>& first, const Time<TTime>& second)
        {
            return new Time<TTime>(__min(first, second));
        }

        static Time<TTime> Max(const Time<TTime>& first, const Time<TTime>& second)
        {
            return new Time<TTime>(__max(first, second));
        }

        bool operator <(const Time<TTime>& first, const Time<TTime>& second)
        {
            return first._time < second._time;
        }

        bool operator >(const Time<TTime>& first, const Time<TTime>& second)
        {
            return first._time > second._time;
        }

        bool operator ==(const Time<TTime>& first, const Time<TTime>& second)
        {
            return first._time == second._time;
        }

        bool operator !=(const Time<TTime>& first, const Time<TTime>& second)
        {
            return first._time != second._time;
        }

        bool operator <=(const Time<TTime>& first, const Time<TTime>& second)
        {
            return first._time <= second._time;
        }

        bool operator >=(const Time<TTime>& first, const Time<TTime>& second)
        {
            return first._time >= second._time;
        }

        Duration<TTime> operator -(const Time<TTime>& first, const Time<TTime>& second)
        {
            return new Duration<TTime>(first._time - second._time);
        }

        Time<TTime> operator -(const Time<TTime>& first, Duration<TTime> second)
        {
            return new Time<TTime>(first._time - second._time);
        }
    };

    // A distance between two Times.
    template<typename T>
    struct Duration<TTime>
    {
        const long _count;

        public Duration(long count)
        {
            WINRT_ASSERT(count >= 0);
            _count = count;
        }

        implicit operator long(Duration<TTime> offset)
        {
            return offset._count;
        }

        implicit operator Duration<TTime>(long value)
        {
            return new Duration<TTime>(value);
        }

        explicit operator Duration<TTime>(Time<TTime> value)
        {
            return new Duration<TTime>(value);
        }

        Duration<TTime> Min(Duration<TTime> first, Duration<TTime> second)
        {
            return new Duration<TTime>(Math.Min(first, second));
        }

        Duration<TTime> operator +(Duration<TTime> first, Duration<TTime> second)
        {
            return new Duration<TTime>(first._time + second._time);
        }

        Duration<TTime> operator -(Duration<TTime> first, Duration<TTime> second)
        {
            return new Duration<TTime>(first._time - second._time);
        }

        Duration<TTime> operator /(Duration<TTime> first, int second)
        {
            return new Duration<TTime>(first._time / second);
        }

        bool operator <(Duration<TTime> first, Duration<TTime> second)
        {
            return first._time < second._time;
        }

        bool operator >(Duration<TTime> first, Duration<TTime> second)
        {
            return first._time > second._time;
        }

        bool operator <=(Duration<TTime> first, Duration<TTime> second)
        {
            return first._time <= second._time;
        }

        bool operator >=(Duration<TTime> first, Duration<TTime> second)
        {
            return first._time >= second._time;
        }

        bool operator ==(Duration<TTime> first, Duration<TTime> second)
        {
            return first._time == second._time;
        }

        bool operator !=(Duration<TTime> first, Duration<TTime> second)
        {
            return first._time != second._time;
        }

        Time<TTime> operator +(Time<TTime> first, Duration<TTime> second)
        {
            return new Time<TTime>(first._time + second._time);
        }

        public override bool Equals(object obj)
        {
            return obj is Duration<TTime> && ((Duration<TTime>)obj)._count == _count;
        }

        public override int GetHashCode()
        {
            return _count.GetHashCode();
        }
    };

    // An interval, defined as a start time and a duration (aka length).
    // 
    // Empty intervals semantically have no InitialTime, and no distinction should be made between empty
    // intervals based on InitialTime.
    template<typename TTime>
    struct Interval
    {
        const Time<TTime> InitialTime;
        const readonly Duration<TTime> Duration;

        Interval(Time<TTime> initialTime, Duration<TTime> duration)
        {
            Check(duration >= 0);

            InitialTime = initialTime;
            Duration = duration;
        }

        static Interval<TTime> Empty() { return new Interval<TTime>(0, 0); }

        bool IsEmpty() { return Duration == 0; }

        Interval<TTime> SubintervalStartingAt(Duration<TTime> offset)
        {
            Check(offset <= Duration);
            return new Interval<TTime>(InitialTime + offset, Duration - offset);
        }

        Interval<TTime> SubintervalOfDuration(Duration<TTime> duration)
        {
            Check(duration <= Duration);
            return new Interval<TTime>(InitialTime, duration);
        }

        Interval<TTime> Intersect(Interval<TTime> other)
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

        bool Contains(Time<TTime> time)
        {
            if (IsEmpty) {
                return false;
            }

            return InitialTime <= time
                && (InitialTime + Duration) > time;
        }
    };

    // A continous distance between two Times.
    template<typename TTime>
    struct ContinuousDuration
    {
        const float _duration;

        ContinuousDuration(float duration)
        {
            _duration = duration;
        }

        ContinuousDuration<TTime> operator *(ContinuousDuration<TTime> duration, float value)
        {
            return new ContinuousDuration<TTime>(value * duration._duration);
        }
        ContinuousDuration<TTime> operator *(float value, ContinuousDuration<TTime> duration)
        {
            return new ContinuousDuration<TTime>(value * duration._duration);
        }
    };
}
