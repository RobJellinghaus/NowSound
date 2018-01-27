// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

namespace NowSound
{
    // 
    // Interface which can consume slice data.
    // 
    public interface Recorder<TTime, TValue>
    where TValue : struct
    {
        // 
        // Record the given data; return true if this recorder is done after recording that data.
        // 
        bool Record(Moment now, Duration<TTime> duration, IntPtr data);

        // 
        // Get the underlying stream, so it can be directly appended.
        // 
        DenseSampleFloatStream Stream{ get; }
    }

    // 
    // Interface which can consume slice data with an associated time.
    // 
    public interface TimedRecorder<TTime, TValue>
        where TValue : struct
        {
            // 
            // Record a sliver from the given source at the given time; return true if this recorder is done.
            // 
            bool Record(Time<TTime> time, TValue[] source, int offset, int width, int stride, int height);
        }
}
