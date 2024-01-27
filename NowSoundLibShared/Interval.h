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
#include "NowSoundTime.h"

namespace NowSound
{
    // Which direction is time going? (and in particular, which direction does an interval go?)
    enum Direction
    {
        Forwards,
        Backwards
    };

    // An interval, defined as a start time, a duration (aka length), and a direction (forward or backward).
    // 
    // Backward intervals have positive times and durations (times and durations are always non-negative),
    // but the interpretation is that the interval is backwards in time, e.g. the start time is before some
    // present time and the end time (interval start time including interval duration) is further before that.
    // 
    // Backwards intervals provide a convenient interface for fetching stream data backwards, which is needed
    // for reverse playback.
    // 
    // When the system was originally implemented, there was a notion of a global time which advanced monotonically
    // towards infinity, and a concept of mapping from that global time to the local time of a loop. Turns out this
    // massively complicated the goal of arbitrarily controlling a loop's local time (reversing it, restarting it
    // etc.). So now Interval is intended to be a *bounded* interval only (e.g. only up to the duration of a stream).
    // If we had dependent typing we might actually carry the containing stream length around, but... no.)
    template<typename TTime>
    class Interval
    {
        // All private data members are semantically const; the only way these ever change is via operator=.
        // (This works better in Rust where there is no assignment operator.)
    private:
        Time<TTime> _time;
        Duration<TTime> _duration;
        Direction _direction;

    public:
        Interval() = delete;

        // Construct the given interval. duration must be non-negative, but initialTime may be any value.
        Interval(Time<TTime> time, Duration<TTime> duration, Direction direction)
            : _time(time), _duration(duration), _direction(direction)
        {
            Check(duration >= 0);
        }

        static Interval<TTime> Empty() { return Interval<TTime>(0, 0, Direction::Forwards); }

        Time<TTime> IntervalTime() const { return _time; }
        Duration<TTime> IntervalDuration() const { return _duration; }

        bool IsEmpty() const { return _duration.Value() == 0; }

        // The rest of the interval after offset.
        // Takes directionality into account.
        // Requires that offset is less than or equal to interval's duration.
        Interval<TTime> Suffix(Duration<TTime> offset) const
        {
            Check(offset <= _duration);

            return Interval<TTime>(_time + offset, _duration - offset, _direction);
        }

        // The interval with the same starting time, up to the given duration.
        // Requires that duration argument is less than or equal to interval's duration.
        Interval<TTime> Prefix(Duration<TTime> duration) const
        {
            Check(duration <= _duration);

            return Interval<TTime>(_time, duration, _direction);
        }

        // Intersection of two intervals (e.g. the interval at which they overlap).
        // This interval must be forwards.
        // If other is backwards, the resulting interval will be forwards.
        Interval<TTime> Intersect(Interval<TTime>& other) const
        {
            Check(_direction == Direction::Forwards);

            // convert other into a forwards interval
            if (other._direction == Direction::Backwards)
            {
                Interval<TTime> otherAsForwards(
                    other.IntervalTime() - other.IntervalDuration(),
                    other.IntervalDuration(),
                    Direction::Forwards);
                other = otherAsForwards;
            }

            Time<TTime> intersectionStart = Time<TTime>::Max(_time, other._time);
            Time<TTime> intersectionEnd = Time<TTime>::Min(_time + _duration, other.IntervalTime() + other.IntervalDuration());

            if (intersectionEnd < intersectionStart)
            {
                return Interval<TTime>::Empty();
            }
            else
            {
                Interval<TTime> result(
                    intersectionStart,
                    intersectionEnd - intersectionStart,
                    Direction::Forwards);

                return result;
            }
        }

        // Does this interval contain the given time?
        // Intervals are semantically closed-open, that is, they contain their IntervalTime but do
        // not contain the IntervalTime just beyond their duration. So an Interval with initial time 1
        // and duration 3 does not contain time 4. (It is the first time after time 1 that the Interval
        // does not contain.)
        bool Contains(Time<TTime> time) const
        {
            // empty intervals contain no times
            if (IsEmpty())
            {
                return false;
            }

            return _time <= time && (_time + _duration) > time;
        }

        Interval<TTime>& operator=(const Interval<TTime>& other)
        {
            _time = other._time;
            _duration = other._duration;
            _direction = other._direction;
            return *this;
        }
    };
}