// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include <cstdint>

#include "pch.h"

#include "BufferAllocator.h"
#include "Time.h"

namespace NowSound
{
    // A reference to a sub-segment of an underlying buffer, indexed by the given TTime type.
    // A Slice is a contiguous segment of Slivers; think of each Sliver as a stereo pair of audio samples,
    // or a video frame, etc., with a Slice being a logically and physically continuous sequence
    // thereof.  Slices do not own their data and are freely copyable, but can become dangling
    // if their underlying stream is trimmed or freed.
    template<typename TTime, typename TValue>
    class Slice
    {
    private:
        // Copy memory from src to dest, using byte offsets.
        static void ArrayCopy(const TValue* src, int64_t srcOffset, const TValue* dest, int64_t destOffset, int64_t length)
        {
            std::memcpy(
                (uint8_t*)dest + destOffset,
                (uint8_t*)src + srcOffset,
                length);
        }

        static Slice<TTime, TValue> s_emptySlice;

        // The backing store; logically divided into slivers.
        // This is borrowed from this slice's containing stream.
        Buf<TValue> _buffer;

        // The number of slivers contained.
        Duration<TTime> _duration;

        // The index to the sliver at which this slice actually begins.
        Duration<TTime> _offset;

        // The count of T values in each sliver in this slice.
        // Slices are composed of multiple Slivers, one per unit of Duration.
        int _sliverCount;

    public:
        static const Slice<TTime, TValue>& Empty() { return s_emptySlice; }

        // Default slice is empty
        Slice() : _duration{}, _offset{}, _sliverCount{}, _buffer{} {}

        Slice(Buf<TValue> buffer, Duration<TTime> offset, Duration<TTime> duration, int sliverCount)
            : _buffer(buffer), _offset(offset), _duration(duration), _sliverCount(sliverCount)
        {
            Check(buffer.Data() != nullptr);
            Check(offset >= 0);
            Check(duration >= 0);
            Check((offset * sliverCount) + (duration * sliverCount) <= buffer.Length()); // TODO: this looks wrong... use GSL std::byte
        }

        Slice(const Buf<TValue> buffer, int sliverSize)
            : _buffer(buffer), _offset(0), _duration(buffer.Data().Length / sliverSize), _sliverCount(sliverSize)
        {}

        Slice(const Slice& other)
            : _buffer(other._buffer), _offset(other._offset), _duration(other.SliceDuration()), _sliverCount(other._sliverCount)
        {}

        Slice& operator=(const Slice& other)
        {
            _buffer = other._buffer;
            _offset = other._offset;
            _duration = other.SliceDuration();
            _sliverCount = other._sliverCount;
            return *this;
        }

        // The number of slivers contained.
        const Duration<TTime> SliceDuration() const { return _duration; }

        // The index to the sliver at which this slice actually begins.
        const Duration<TTime> Offset() const { return _offset; }

        // The size of each sliver in this slice; a count of T.
        // Slices are composed of multiple Slivers, one per unit of Duration.
        int SliverCount() const { return _sliverCount; }

        bool IsEmpty() const { return SliceDuration() == 0; }

        // TODO: make this private
        Buf<TValue> Buffer() const { return _buffer; }

        // Get a single value out of the slice at the given offset, sub-indexed in the slice by the given sub-index.
        // Can't get from an empty slice.
        TValue& Get(Duration<TTime> offset, int subindex) const
        {
            Check(!IsEmpty());
            Check(totalOffset * _sliverCount < _buffer.Data().Length);
            Duration<TTime> totalOffset = _offset + offset;
            long finalOffset = totalOffset * _sliverCount + subindex;
            return _buffer.Data()[finalOffset];
        }

        // Get a portion of this Slice, starting at the given offset, for the given duration.
        Slice<TTime, TValue> Subslice(Duration<TTime> initialOffset, Duration<TTime> duration) const
        {
            Check(initialOffset >= 0); // can't slice before the beginning of this slice
            Check(_duration >= 0); // must be nonnegative count
            Check(initialOffset + duration <= _duration); // can't slice beyond the end
            return Slice<TTime, TValue>(_buffer, Offset() + initialOffset, duration, _sliverCount);
        }

        // Get the rest of this Slice starting at the given offset.
        Slice<TTime, TValue> SubsliceStartingAt(Duration<TTime> initialOffset) const
        {
            return Subslice(initialOffset, _duration - initialOffset);
        }

        TValue* OffsetPointer() { return Buffer()->Data + (_offset * _sliverCount); }

        // Get the prefix of this Slice starting at offset 0 and extending for the requested duration.
        Slice<TTime, TValue> SubsliceOfDuration(Duration<TTime> duration) const
        {
            return Subslice(0, duration);
        }

        size_t SliverSizeInBytes() const { return sizeof(TValue) * _sliverCount; }
        size_t SizeInBytes() const { return SliverSizeInBytes() * _duration.Value(); }

        // Copy this slice's data into destination; destination must be long enough.
        void CopyTo(Slice<TTime, TValue>& destination) const
        {
            Check(destination.SliceDuration() >= _duration);
            Check(destination._sliverCount == _sliverCount);

            // TODO: support reversed copies etc.
            ArrayCopy(_buffer.Data(),
                _offset.Value() * _sliverCount,
                destination._buffer.Data(),
                destination._offset.Value() * _sliverCount,
                SizeInBytes());
        }

        void CopyTo(TValue* dest) const
        {
            ArrayCopy(_buffer.Data(), _offset.Value() * _sliverCount, dest, 0, SizeInBytes());
        }

        // Copy data from the source, replacing all data in this slice.
        void CopyFrom(TValue* source)
        {
            ArrayCopy(source, 0, _buffer.Data(), _offset.Value() * _sliverCount, SizeInBytes());
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
            return Slice<TTime, TValue>(_buffer, _offset, _duration + next.SliceDuration(), _sliverCount);
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

        TimedSlice(Time<TTime> startTime, Slice<TTime, TValue> slice) : _time(startTime), _value(slice)
        {
        }

        TimedSlice& operator=(const TimedSlice& other)
        {
            _time = other._time;
            _value = other._value;
            return *this;
        }

        Interval<TTime> SliceInterval() const { return Interval<TTime>(_time, _value.SliceDuration()); }

        bool operator<(const TimedSlice<TTime, TValue>& other) const
        {
            return _time < other._time;
        }
    };
}
