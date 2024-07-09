// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

// Why isn't this file just named "Time.h"?
// FUNNY YOU SHOULD ASK:
// https://stackoverflow.com/questions/23907008/compilation-error-error-c2039-clock-t-is-not-a-member-of-global-namespace

#pragma once

#include "stdafx.h"

#include "math.h"
#include "stdint.h" // for int64_t

#include "Check.h"

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

    template<typename TTime>
    class ContinuousTime;

    template<typename TTime>
    class ContinuousDuration;

    // Time parameterized on some underlying unit of measurement (such as those above).
    // This theoretically could also provide a timing base (e.g. number of units per other-unit),
    // but currently does not.
    // The rationale for these types and the boilerplate around them is that plain numeric types
    // in a multimedia program always lead to confusion -- what is that "int" exactly?  These types
    // eliminate these confusions and help to catch real bugs.
    template<typename TTime>
    class Time
    {
    private:
        // The number of TTime units represented by this time.
        int64_t _value;

    public:
        Time() : _value{} {};

        Time(int64_t value) : _value(value)
        {
        }

        int64_t Value() const { return _value; }

        static Time<TTime> Min(const Time<TTime>& first, const Time<TTime>& second)
        {
            return Time<TTime>(__min(first.Value(), second.Value()));
        }

        static Time<TTime> Max(const Time<TTime>& first, const Time<TTime>& second)
        {
            return Time<TTime>(__max(first.Value(), second.Value()));
        }

        Time<TTime>& operator=(const Time<TTime>& other)
        {
            _value = other.Value();
            return *this;
        }

        bool operator <(const Time<TTime>& second) const
        {
            return _value < second.Value();
        }

        bool operator >(const Time<TTime>& second) const
        {
            return _value > second.Value();
        }

        bool operator ==(const Time<TTime>& second) const
        {
            return _value == second.Value();
        }

        bool operator !=(const Time<TTime>& second) const
        {
            return _value != second.Value();
        }

        bool operator <=(const Time<TTime>& second) const
        {
            return _value <= second.Value();
        }

        bool operator >=(const Time<TTime>& second) const
        {
            return _value >= second.Value();
        }

        ContinuousTime<TTime> AsContinuous() const { return ContinuousTime<TTime>(static_cast<float>(Value())); }
    };

    // A distance between two Times.
    template<typename TTime>
    class Duration
    {
    private:
        int64_t _value;

    public:
        Duration() : _value{} {};

        Duration(int64_t duration) : _value(duration)
        { }

        int64_t Value() const { return _value; }

        static Duration<TTime> Min(Duration<TTime> first, Duration<TTime> second)
        {
            return new Duration<TTime>(Math.Min(first, second));
        }

        Duration<TTime>& operator=(const Duration<TTime>& other)
        {
            _value = other.Value();
            return *this;
        }

        Duration<TTime> operator /(float second) const
        {
            return Duration<TTime>((int64_t)(_value / second));
        }

        Duration<TTime> operator *(float second) const
        {
            return Duration<TTime>((int64_t)(_value * second));
        }

        Duration<TTime> operator *(int second) const
        {
            return Duration<TTime>(_value * second);
        }

        bool operator <(const Duration<TTime>& second) const
        {
            return _value < second.Value();
        }

        bool operator >(const Duration<TTime>& second) const
        {
            return _value > second.Value();
        }

        bool operator <=(const Duration<TTime>& second) const
        {
            return _value <= second.Value();
        }

        bool operator >=(const Duration<TTime>& second) const
        {
            return _value >= second.Value();
        }

        bool operator ==(const Duration<TTime>& second) const
        {
            return _value == second.Value();
        }

        bool operator !=(const Duration<TTime>& second) const
        {
            return _value != second.Value();
        }

        ContinuousDuration<TTime> AsContinuous() const { return ContinuousDuration<TTime>(static_cast<float>(Value())); }
    };

    template<typename TTime>
    Duration<TTime> operator -(Time<TTime> first, Time<TTime> second)
    {
        return Duration<TTime>(first.Value() - second.Value());
    }

    template<typename TTime>
    Time<TTime> operator -(Time<TTime> first, Duration<TTime> second)
    {
        return Time<TTime>(first.Value() - second.Value());
    }

    template<typename TTime>
    Duration<TTime> operator +(Duration<TTime> first, Duration<TTime> second)
    {
        return Duration<TTime>(first.Value() + second.Value());
    }

    template<typename TTime>
    Duration<TTime> operator -(Duration<TTime> first, Duration<TTime> second)
    {
        return Duration<TTime>(first.Value() - second.Value());
    }

    template<typename TTime>
    Time<TTime> operator +(Time<TTime> first, Duration<TTime> second)
    {
        return Time<TTime>(first.Value() + second.Value());
    }

    template<typename TTime>
    Time<TTime> operator +(Duration<TTime> first, Time<TTime> second)
    {
        return Time<TTime>(first._duration + second.Value());
    }

    template<typename TTime>
    Time<TTime> operator -(Duration<TTime> first, Time<TTime> second)
    {
        return Time<TTime>(first._duration - second.Value());
    }


    // A continous Time. The main use for this is to keep exact track of how many fractional
    // samples have been played, modulo the length of a loop. This enables handling rounding
    // properly when wrapping around a loop.
    template<typename TTime>
    class ContinuousTime
    {
    private:
        float _value;

    public:
        ContinuousTime() = delete;

        ContinuousTime(float value) : _value(value)
        {
            Check(value >= 0);
        }

        ContinuousTime(const ContinuousTime<TTime>& other) : _value(other.Value())
        {
        }

        float Value() const { return _value; }

        ContinuousTime<TTime>& operator =(const ContinuousTime<TTime>& other)
        {
            _value = other._value;
            return *this;
        }

        ContinuousTime<TTime> operator *(float value) const
        {
            return new ContinuousTime<TTime>(value * _value);
        }

        // The integer part of this, as a (non-continuous) Time.
        Time<TTime> RoundedDown() const { return Time<TTime>((int)std::floorf(Value())); }
    };

    // A continous distance between two Times.
    template<typename TTime>
    class ContinuousDuration
    {
    private:
        float _value;

    public:
        ContinuousDuration() = delete;

        ContinuousDuration(float value) : _value(value)
        {
            Check(value >= 0);
        }

        ContinuousDuration(const ContinuousDuration<TTime>& other) : _value(other.Value())
        {
        }

        float Value() const { return _value; }

        ContinuousDuration<TTime>& operator =(const ContinuousDuration<TTime>& other)
        {
            _value = other._value;
            return *this;
        }

        ContinuousDuration<TTime> operator *(float value) const
        {
            return new ContinuousDuration<TTime>(value * _value);
        }

        // The integer part of this, as a (non-continuous) Duration.
        Duration<TTime> RoundedDown() const { return Duration<TTime>(static_cast<int64_t>(std::floorf(Value()))); }
        // The rounded-up value of this, as a (non-continuous) Duration.
        Duration<TTime> RoundedUp() const { return Duration<TTime>(static_cast<int64_t>(std::ceilf(Value()))); }
    };

    template<typename TTime>
    ContinuousTime<TTime> operator +(ContinuousTime<TTime> first, ContinuousDuration<TTime> second)
    {
        return ContinuousTime<TTime>(first.Value() + second.Value());
    }

    template<typename TTime>
    ContinuousDuration<TTime> operator +(ContinuousDuration<TTime> first, ContinuousDuration<TTime> second)
    {
        return ContinuousDuration<TTime>(first.Value() + second.Value());
    }

    template<typename TTime>
    ContinuousTime<TTime> operator -(ContinuousTime<TTime> first, ContinuousDuration<TTime> second)
    {
        return ContinuousTime<TTime>(first.Value() - second.Value());
    }

    template<typename TTime>
    ContinuousDuration<TTime> operator -(ContinuousDuration<TTime> first, ContinuousDuration<TTime> second)
    {
        return ContinuousDuration<TTime>(first.Value() - second.Value());
    }

}
