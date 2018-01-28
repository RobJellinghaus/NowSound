// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "Time.h"
#include "SliceStream.h"

namespace NowSound
{
    // Interface which can consume slice data.
    template<typename TTime, typename TValue>
    class Recorder
    {
    public:
        // 
        // Record the given data; return true if this recorder is done after recording that data.
        // 
        virtual bool Record(Moment now, Duration<TTime> duration, IntPtr data) = 0;

        // 
        // Get the underlying stream, so it can be directly appended.
        // 
        virtual DenseSampleFloatStream Stream() = 0;
    };

    // 
    // Interface which can consume slice data with an associated time.
    // 
    template<typename TTime, typename TValue>
    class TimedRecorder
    {
        // 
        // Record a sliver from the given source at the given time; return true if this recorder is done.
        // 
        virtual bool Record(Time<TTime> time, TValue[] source, int offset, int width, int stride, int height);
    }
}
