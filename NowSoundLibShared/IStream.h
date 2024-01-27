#pragma once

// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"
#include "Interval.h"
#include "NowSoundTime.h"

namespace NowSound
{
    // Class which provides an interface to the Stream functions that mappers need.
    template<typename TTime>
    class IStream
    {
    public:
        // Discrete duration of stream; increases steadily during recording, ends up with the value Math.Ceil(ExactDuration()).
        virtual Duration<TTime> DiscreteDuration() const = 0;
        // Continuous duration of stream; cannot be called until IsShut().
        virtual ContinuousDuration<TTime> ExactDuration() const = 0;
        // Interval of stream.
        Interval<TTime> DiscreteInterval() const { return Interval<TTime>(Time<TTime>(0), DiscreteDuration()); }
        // Is the stream shut (that is, no longer accepting appends, and has begun looping)?
        virtual bool IsShut() const = 0;
    };
}
