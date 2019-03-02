// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"
#include "NowSoundTime.h"
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
        virtual bool Record(Duration<TTime> duration, TValue* data) = 0;
    };

    // Interface which can consume slice data with an associated time.
    template<typename TTime, typename TValue>
    class ITimedRecorder
    {
        // Record a sliver from the given source; return true if this recorder will continue recording.
        virtual bool Record(TValue* source, int offset, int width, int stride, int height) = 0;
    };

    // Helper class which records into a non-owned audio stream.
    template<typename TTime, typename TValue>
    class StreamRecorder : public IRecorder<TTime, TValue>
    {
    private:
        BufferedSliceStream<TTime, TValue>*_stream;

    public:
        StreamRecorder(BufferedSliceStream<TTime, TValue>* stream)
            : _stream(stream)
        {}

        virtual bool Record(Duration<TTime> duration, TValue* data)
        {
            _stream->Append(duration, data);
            return true;
        }
    };
}
