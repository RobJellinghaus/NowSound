// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include <cstdint>

#include "stdafx.h"

#include "BufferAllocator.h"
#include "NowSoundTime.h"

namespace NowSound
{
    // A Slice is reference to a sub-segment of an underlying buffer.
    //
    // A Slice is a contiguous segment of individual slices; think of each individual slice as a stereo
    // pair of audio samples, or a video frame, etc., with a Slice being a logically and physically
    // contiguous sequence thereof.
    // 
    // Slices do not own their data and are freely copyable, but can become dangling if their underlying
    // stream is trimmed or freed.
    //
    // Slices are always forwards in time. The GetSliceIntersecting(Interval) method accepts backwards
    // Intervals, but returns slices that are always forwards in time.
    template<typename TTime, typename TValue>
    class Slice
    {
    private:
        // Copy memory from src to dest.  All non-pointer arguments are counts of T values, *not* byte counts.
        static void ArrayCopy(const TValue* src, int64_t srcOffset, const TValue* dest, int64_t destOffset, int64_t count)
        {
            std::memcpy(
                (uint8_t*)dest + (destOffset * sizeof(TValue)),
                (uint8_t*)src + (srcOffset * sizeof(TValue)),
                count * sizeof(TValue));
        }

        static Slice<TTime, TValue> s_emptySlice;

        // The backing store; logically divided into individual slices, each containing _sliceSize values.
        // This is borrowed from this slice's containing stream.
        Buf<TValue> _buffer;

        // The number of individual slices contained.
        Duration<TTime> _duration;

        // The index of the first individual slice.
        Duration<TTime> _offset;

        // The count of T values in each individual slice.
        int _sliceSize;

    public:
        static const Slice<TTime, TValue>& Empty() { return s_emptySlice; }

        // Default slice is empty
        Slice() : _duration{}, _offset{}, _sliceSize{}, _buffer{} {}

        Slice(const Buf<TValue>& buffer, Duration<TTime> offset, Duration<TTime> duration, int sliceSize)
            : _buffer(buffer), _offset(offset), _duration(duration), _sliceSize(sliceSize)
        {
            Check(buffer.Data() != nullptr);
            Check(offset >= 0);
            Check(duration >= 0);
            Check((offset * sliceSize) + (duration * sliceSize) <= buffer.Length()); // TODO: this looks wrong... use GSL std::byte
        }

        Slice(const Buf<TValue>& buffer, int sliceSize)
            : _buffer(buffer), _offset(0), _duration(buffer.Length() / sliceSize), _sliceSize(sliceSize)
        {}

        Slice(const Slice& other)
            : _buffer(other._buffer), _offset(other._offset), _duration(other.SliceDuration()), _sliceSize(other._sliceSize)
        {}

        Slice& operator=(const Slice& other)
        {
            _buffer = other._buffer;
            _offset = other._offset;
            _duration = other.SliceDuration();
            _sliceSize = other._sliceSize;
            return *this;
        }

        // The number of individual slices that this Slice refers to.
        const Duration<TTime> SliceDuration() const { return _duration; }

        // The index of the first individual slice that this Slice refers to.
        const Duration<TTime> Offset() const { return _offset; }

        // The size of each individual slice; a count of T.
        int SliceSize() const { return _sliceSize; }

        bool IsEmpty() const { return SliceDuration() == 0; }

        // TODO: make this private
        Buf<TValue> Buffer() const { return _buffer; }

        // Get a single value out of the slice at the given offset, selecting the given value by index.
        // Can't get from an empty slice.
        TValue& Get(Duration<TTime> offset, int sliceInnerIndex) const
        {
            Check(!IsEmpty());
            Duration<TTime> totalOffset = _offset + offset;
            Check(totalOffset.Value() * _sliceSize < _buffer.Length());
            int64_t finalOffset = totalOffset.Value() * _sliceSize + sliceInnerIndex;
            return _buffer.Data()[finalOffset];
        }

        // Get a portion of this Slice, starting at the given offset, for the given duration.
        Slice<TTime, TValue> Subslice(Duration<TTime> initialOffset, Duration<TTime> duration) const
        {
            Check(initialOffset >= 0); // can't slice before the beginning of this slice
            Check(_duration >= 0); // must be nonnegative count
            Check(initialOffset + duration <= _duration); // can't slice beyond the end
            return Slice<TTime, TValue>(_buffer, Offset() + initialOffset, duration, _sliceSize);
        }

        // Get the rest of this Slice starting at the given offset.
        Slice<TTime, TValue> SubsliceStartingAt(Duration<TTime> initialOffset) const
        {
            return Subslice(initialOffset, _duration - initialOffset);
        }

        // Return a pointer to the start of the data addressed by this slice.
        TValue* OffsetPointer() { return Buffer().Data() + (_offset.Value() * _sliceSize); }

        // Get the prefix of this Slice starting at offset 0 and extending for the requested duration.
        Slice<TTime, TValue> SubsliceOfDuration(Duration<TTime> duration) const
        {
            return Subslice(0, duration);
        }

        // Copy this slice's data into destination; destination must be long enough.
        void CopyTo(Slice<TTime, TValue>& destination) const
        {
            Check(destination.SliceDuration() >= _duration);
            Check(destination._sliceSize == _sliceSize);

            // TODO: support reversed copies etc.
            ArrayCopy(_buffer.Data(),
                _offset.Value() * _sliceSize,
                destination._buffer.Data(),
                destination._offset.Value() * _sliceSize,
                _duration.Value() * _sliceSize);
        }

        void CopyTo(TValue* dest) const
        {
            ArrayCopy(_buffer.Data(), _offset.Value() * _sliceSize, dest, 0, _duration.Value() * _sliceSize);
        }

        // Copy data from the source, replacing all data in this slice.
        void CopyFrom(const TValue* source)
        {
            ArrayCopy(source, 0, _buffer.Data(), _offset.Value() * _sliceSize, _duration.Value() * _sliceSize);
        }

        // Copy data from the source, replacing only a portion of the slice.
        void CopyFrom(const TValue* source, int innerSliceIndex, int length)
        {
            ArrayCopy(source, 0, _buffer.Data(), _offset.Value() * _sliceSize + innerSliceIndex, length);
        }

        // Are these samples adjacent in their underlying storage?
        bool Precedes(const Slice<TTime, TValue>& next) const
        {
            return _buffer.Data() == next._buffer.Data() && _offset + _duration == next._offset;
        }

        // Merge two adjacent samples into a single sample.
        // Precedes(next) must be true.
        Slice<TTime, TValue> UnionWith(const Slice<TTime, TValue>& next) const
        {
            Check(Precedes(next));
            return Slice<TTime, TValue>(_buffer, _offset, _duration + next.SliceDuration(), _sliceSize);
        }

        // Equality comparison.
        bool Equals(const Slice<TTime, TValue>& other) const
        {
            return Buffer.Equals(other.Buffer) && Offset == other.Offset && _duration == other.SliceDuration();
        }
    };

    template<typename TTime, typename TValue>
    Slice<TTime, TValue> Slice<TTime, TValue>::s_emptySlice{};

    // A slice with an absolute initial time associated with it.
    // In the case of BufferedStreams, the first TimedSlice's InitialTime will be the InitialTime of the stream itself.
    // TODO: double-check that that still makes sense in the BufferedSliceStream implementation (it probably does).
    template<typename TTime, typename TValue>
    struct TimedSlice
    {
    private:
        Time<TTime> _time;
        Slice<TTime, TValue> _value;

    public:
        const Time<TTime> InitialTime() const { return _time; }

        const Slice<TTime, TValue>& Value() const { return _value; }

        // Use with caution, and only in exceptional circumstances where modifying slice data is desirable.
        Slice<TTime, TValue>& NonConstValue() { return _value; }

        void ChangeInitialTimeBy(Duration<TTime> delta)
        {
            _time = _time + delta;
        }

        TimedSlice(Time<TTime> startTime, Slice<TTime, TValue> slice) : _time(startTime), _value(slice)
        {
        }

        TimedSlice& operator=(const TimedSlice& other)
        {
            _time = other._time;
            _value = other._value;
            return *this;
        }

        Interval<TTime> SliceInterval() const { return Interval<TTime>(_time, _value.SliceDuration(), Direction::Forwards); }

        bool operator<(const TimedSlice<TTime, TValue>& other) const
        {
            return _time < other._time;
        }
    };
}
