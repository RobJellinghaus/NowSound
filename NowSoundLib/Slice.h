#pragma once

// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include "BufferAllocator.h"
#include "Time.h"

namespace NowSound
{
    // A reference to a sub-segment of an underlying buffer, indexed by the given TTime type.
    // A Slice is a contiguous segment of Slivers; think of each Sliver as a stereo pair of audio samples,
    // or a video frame, etc., with a Slice being a logically and physically continuous sequence
    // thereof.
    template<typename TTime, typename TValue>
    class Slice
    {
    private:
        // The backing store; logically divided into slivers.
        // This is a borrowed reference; the Buf's BufferAllocator owns it.
        const Buf<TValue>* _buffer;

    public:
        // The number of slivers contained.
        const Duration<TTime> _duration;

        // The index to the sliver at which this slice actually begins.
        const Duration<TTime> _offset;

        // The size of each sliver in this slice; a count of T.
        // Slices are composed of multiple Slivers, one per unit of Duration.
        const int _sliverSize;

        Slice(Buf<TValue>* buffer, Duration<TTime> offset, Duration<TTime> duration, int sliverSize)
            : _buffer(buffer), _offset(offset), _duration(duration), _sliverSize(sliverSize)
        {
            Check(buffer.Data != null);
            Check(offset >= 0);
            Check(duration >= 0);
            Check((offset * sliverSize) + (duration * sliverSize) <= buffer.Data.Length);

            _buffer = buffer;
            _duration = duration;
            offset = offset;
            _sliverSize = sliverSize;
        }

        Slice(Buf<TValue> buffer, int sliverSize)
            : this(buffer, 0, (buffer.Data.Length / sliverSize), sliverSize)
        {
        }

        static Buf<TValue> s_emptyBuf(0, new TValue[0]);

        static const Slice<TTime, TValue> Empty(&s_emptyBuf, 0, 0, 0);

        bool IsEmpty() { return Duration == 0; }

        // For use by extension methods only
        Buf<TValue>* Buffer() { return _buffer; }

        TValue& operator[](Duration<TTime> offset, int subindex)
        {
            Duration<TTime> totalOffset = _offset + offset;
            Check(totalOffset * _sliverSize < Buffer.Data.Length);
            long finalOffset = totalOffset * _sliverSize + subindex;
            return _buffer.Data[finalOffset];
        }

        // Get a portion of this Slice, starting at the given offset, for the given duration.
        Slice<TTime, TValue> Subslice(Duration<TTime> initialOffset, Duration<TTime> duration)
        {
            Check(initialOffset >= 0); // can't slice before the beginning of this slice
            Check(_duration >= 0); // must be nonnegative count
            Check(initialOffset + duration <= _duration); // can't slice beyond the end
            return new Slice<TTime, TValue>(_buffer, Offset + initialOffset, duration, _sliverSize);
        }

        // Get the rest of this Slice starting at the given offset.
        Slice<TTime, TValue> SubsliceStartingAt(Duration<TTime> initialOffset)
        {
            return Subslice(initialOffset, _duration - initialOffset);
        }

        // Get the prefix of this Slice starting at offset 0 and extending for the requested duration.
        Slice<TTime, TValue> SubsliceOfDuration(Duration<TTime> duration)
        {
            return Subslice(0, duration);
        }

        // Copy this slice's data into destination; destination must be long enough.
        void CopyTo(Slice<TTime, TValue>& destination)
        {
            Check(destination._duration >= _duration);
            Check(destination._sliverSize == _sliverSize);

            // TODO: support backwards copies etc.
            Array.Copy(
                _buffer.Data,
                (int)_offset * _sliverSize,
                destination._buffer.Data,
                (int)destination._offset * destination._sliverSize,
                (int)_duration * _sliverSize);
        }

        void CopyFrom(TValue[] source, int sourceOffset, int destinationSubIndex, int subWidth)
        {
            Check((int)(sourceOffset + subWidth) <= source.Length);
            int destinationOffset = (int)(_offset * _sliverSize + destinationSubIndex);
            Check(destinationOffset + subWidth <= _buffer.Data.Length);

            Array.Copy(
                source,
                (int)sourceOffset,
                _buffer.Data,
                destinationOffset,
                subWidth);
        }

        // Are these samples adjacent in their underlying storage?
        bool Precedes(const Slice<TTime, TValue>& next)
        {
            return _buffer.Data == next._buffer.Data && _offset + _duration == next._offset;
        }

        // Merge two adjacent samples into a single sample.
        public Slice<TTime, TValue> UnionWith(const Slice<TTime, TValue>& next) const
        {
            Contract.Assert(Precedes(next));
            return new Slice<TTime, TValue>(_buffer, _offset, _duration + next._duration, _sliverSize);
        }

        // Equality comparison; deliberately does not implement Equals(object) as this would cause slice boxing.
        bool Equals(const Slice<TTime, TValue>& other)
        {
            return Buffer.Equals(other.Buffer) && Offset == other.Offset && _duration == other._duration;
        }
    };

    // A slice with an absolute initial time associated with it.
    // 
    // In the case of BufferedStreams, the first TimedSlice's InitialTime will be the InitialTime
    // of the stream itself.
    template<typename TTime, typename TValue>
    struct TimedSlice
    {
        const Time<TTime> InitialTime;
        const Slice<TTime, TValue> Slice;

        TimedSlice(Time<TTime> startTime, Slice<TTime, TValue> slice) : InitialTime(startTime), Slice(slice)
        {
            InitialTime = startTime;
            Slice = slice;
        }

        Interval<TTime> Interval() { return new Interval<TTime>(InitialTime, Slice._duration); }
    };
}
