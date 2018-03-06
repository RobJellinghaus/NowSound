// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"
#include "Time.h"
#include "SliceStream.h"

namespace NowSound
{
    // Interface which can consume slice data.
    template<typename TTime, typename TValue>
    class IRecorder
    {
    public:
        // Record the given data; return true if this recorder will continue recording.
        // If false is returned, this recorder will never be invoked again.
        virtual bool Record(Time<TTime> now, Duration<TTime> duration, TValue* data) = 0;
    };

    // 
    // Interface which can consume slice data with an associated time.
    // 
    template<typename TTime, typename TValue>
    class ITimedRecorder
    {
        // Record a sliver from the given source at the given time; return true if this recorder will continue recording.
        // 
        virtual bool Record(Time<TTime> time, TValue* source, int offset, int width, int stride, int height) = 0;
    };
}
