// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "IntervalMapper.h"
#include "Slice.h"
#include "Time.h"

namespace NowSound
{
    // A stream of data, which can be Shut, at which point it acquires a floating-point ContinuousDuration.
    // 
    // Streams may be Open (in which case more data may be appended to them), or Shut (in which case they will
    // not change again).
    // 
    // Streams may have varying internal policies for mapping time to underlying data, and may form hierarchies
    // internally.
    // 
    // Streams have a SliverSize which denotes a larger granularity within the Stream's data.
    // A SliverSize of N represents that each element in the Stream logically consists of N contiguous
    // TValue entries in the stream's backing store; such a contiguous group is called a sliver.  
    // A Stream with duration 1 has exactly one sliver of data. 
    template<typename TTime, typename TValue>
    class SliceStream : IStream<TTime>
    {
        // The initial time of this Stream.
        // 
        // Note that this is discrete.  We don't consider a sub-sample's worth of error in the start time to be
        // significant.  The loop duration, on the other hand, is iterated so often that error can and does
        // accumulate; hence, ContinuousDuration, defined only once shut.
        // </remarks>
    protected:
        Time<TTime> _initialTime;

        // The floating-point duration of this stream in terms of samples; only valid once shut.
        // 
        // This allows streams to have lengths measured in fractional samples, which prevents roundoff error from
        // causing clock drift when using unevenly divisible BPM values and looping for long periods.
        ContinuousDuration<AudioSample> _continuousDuration;

        // As with Slice<typeparam name="TValue"></typeparam>, this defines the number of T values in an
        // individual slice.
        int _sliverSize;

        // Is this stream shut?
        bool _isShut;

        SliceStream(Time<TTime> initialTime, int sliverSize)
        {
            _initialTime = initialTime;
            _sliverSize = sliverSize;
        }

    public:
        bool IsShut() const { return _isShut; }

        // The starting time of this Stream.
        virtual Time<TTime> InitialTime() const { return _initialTime; }

        // The floating-point-accurate duration of this stream; only valid once shut.
        // This may have a fractional part if the BPM of the stream can't be evenly divided into
        // the sample rate.
        ContinuousDuration<AudioSample> ExactDuration() const { return _continuousDuration; }

        // Shut the stream; no further appends may be accepted.
        // 
        // finalDuration is the possibly fractional duration to be associated with the stream;
        // must be strictly equal to, or less than one sample smaller than, the discrete duration.
        virtual void Shut(ContinuousDuration<AudioSample> finalDuration)
        {
            Check(!IsShut());
            _isShut = true;
            _continuousDuration = finalDuration;
        }

        // Drop this stream and all its owned data.
        // 
        // This MAY need to become a ref counting structure if we want stream dependencies.
        virtual void Dispose() = 0;
    };

    // A stream of data, accessed through consecutive, densely sequenced Slices.
    template<typename TTime, typename TValue>
    class DenseSliceStream : public SliceStream<TTime, TValue>
    {
        // The discrete duration of this stream; always exactly equal to the sum of the durations of all contained slices.
    protected:
        Duration<TTime> _discreteDuration;

        // The mapper that converts absolute time into relative time for this stream.
        // This is held by a unique_ptr to allow for polymorphism.
        std::unique_ptr<IntervalMapper<TTime>> _intervalMapper;

        DenseSliceStream(Time<TTime> initialTime, int sliverSize)
            : base(initialTime, sliverSize)
        {
        }

    public:
        // The discrete duration of this stream; always exactly equal to the number of timepoints appended.
        virtual Duration<TTime> DiscreteDuration() const { return _discreteDuration; }

        Interval<TTime> DiscreteInterval() const { return new Interval<TTime>(InitialTime(), DiscreteDuration()); }

        const IntervalMapper<TTime>& Mapper() const { return _intervalMapper; }
        void Mapper(IntervalMapper<TTime>&& value) { _intervalMapper = std::move(value); }

        // Shut the stream; no further appends may be accepted.
        // 
        // finalDuration is the possibly fractional duration to be associated with the stream;
        // must be strictly equal to, or less than one sample smaller than, the discrete duration.</param>
        virtual void Shut(ContinuousDuration<AudioSample> finalDuration)
        {
            Check(!this->IsShut());
            // Should always have as many samples as the rounded-up finalDuration.
            // The precise time matching behavior is that a loop will play either Math.Floor(finalDuration)
            // or Math.Ceiling(finalDuration) samples on each iteration, such that it remains perfectly in
            // time with finalDuration's fractional value.  So, a shut loop should have DiscreteDuration
            // equal to rounded-up ContinuousDuration.
            Check((int)std::ceil(finalDuration.Value()) == DiscreteDuration().Value());
            SliceStream<TTime, TValue>::Shut(finalDuration);
        }

        // Get a reference to the next slice at the given time, up to the given duration if possible, or the
        // largest available slice if not.
        // 
        // If the interval IsEmpty, return an empty slice.
        virtual Slice<TTime, TValue> GetNextSliceAt(Interval<TTime> sourceInterval) const = 0;

        // Append contiguous data.
        // This must not be shut yet.
        virtual void Append(Slice<TTime, TValue> source) = 0;

        // Append the given duration's worth of slices from the given pointer.
        // This must not be shut yet.
        virtual void Append(Duration<TTime> duration, TValue* p) = 0;

        // Copy the given interval of this stream to the destination.
        virtual void CopyTo(Interval<TTime> sourceInterval, TValue* destination) const = 0;

        /*
        // Copy the given interval of this stream to the destination.
        virtual void CopyTo(Interval<TTime> sourceInterval, DenseSliceStream<TTime, TValue> destination) const = 0;
        */
    };

    // A stream that buffers some amount of data in memory.
    template<typename TTime, typename TValue>
    class BufferedSliceStream : public DenseSliceStream<TTime, TValue>
    {
    private:
        // Allocator for obtaining buffers; borrowed from application.
        const BufferAllocator<TValue>* _allocator;

        // The slices making up the buffered data itself.
        // The InitialTime of each entry in this list must exactly equal the InitialTime + Duration of the
        // previous entry; in other words, these are densely arranged in time.
        // Note that slices borrow buffer references from their containing stream.
        std::vector<TimedSlice<TTime, TValue>> _data{};

        // The maximum amount that this stream will buffer while it is open; more appends will cause
        // earlier data to be dropped.  If 0, no buffering limit will be enforced.
        Duration<TTime> _maxBufferedDuration;

        // Temporary space for, e.g., the IntPtr Append method.
        // This buffer is owned by this stream.
        Buf<TValue> _tempBuffer; // = new Buf<TValue>(-1, new TValue[1024]); // -1 id = temp buf

        // This is the vector of buffers appended in the stream thus far; the last one is the current append buffer.
        // This vector owns the buffers within it; ownership is transferred from allocator to stream whenever a
        // new append buffer is needed.
        std::vector<Buf<TValue>> _buffers;

        // This is the remaining not-yet-allocated portion of the current append buffer (the last in _buffers).
        Slice<TTime, TValue> _remainingFreeSlice;

        bool _useContinuousLoopingMapper;

        void EnsureFreeBuffer()
        {
            if (_remainingFreeSlice.IsEmpty())
            {
                // allocate a new buffer and transfer ownership of it to _buffers
                _buffers.emplace_back(std::move(_allocator->Allocate()));

                // get a borrowed pointer to the current append buffer
                Buf<TValue>* appendBuffer = &_buffers.at(_buffers.size() - 1);

                _remainingFreeSlice = new Slice<TTime, TValue>(
                    appendBuffer,
                    0,
                    appendBuffer->Data.Length / SliverSize,
                    SliverSize);
            }
        }

    public:
        BufferedSliceStream(
            Time<TTime> initialTime,
            BufferAllocator<TValue>* allocator,
            int sliverSize,
            Duration<TTime> maxBufferedDuration,
            bool useContinuousLoopingMapper)
            : base(initialTime, sliverSize),
            _allocator(allocator),
            _buffers{},
            _remainingFreeSlice{},
            _maxBufferedDuration(maxBufferedDuration),
            _useContinuousLoopingMapper(useContinuousLoopingMapper),
            // when appending, we always start out with identity mapping
            _intervalMapper(new IdentityIntervalMapper<TTime>(this))
        {
        }

        // On destruction, return all buffers to free list
        // TODO: does this need locking and/or thread checks?
        ~BufferedSliceStream()
        {
            for (int i = 0; i < _buffers.size(); i++)
            {
                // transfer ownership of each buffer back to allocator
                _allocator->Free(std::move(_buffers.at(i)));
            }
        }

        virtual void Shut(ContinuousDuration<AudioSample> finalDuration)
        {
            base.Shut(finalDuration);
            // swap out our mappers, we're looping now
            if (_useContinuousLoopingMapper)
            {
                _intervalMapper = new LoopingIntervalMapper<TTime, TValue>(this);
            }
            else
            {
                _intervalMapper = new SimpleLoopingIntervalMapper<TTime, TValue>(this);
            }

#if SPAMAUDIO
            foreach(TimedSlice<TTime, TValue> timedSlice in _data) {
                Spam.Audio.WriteLine("BufferedSliceStream.Shut: next slice time " + timedSlice.InitialTime + ", slice " + timedSlice.Slice);
            }
#endif
        }

        // Return a temporary buffer slice of the given duration or the max temp buffer size, whichever is lower.
        Slice<TTime, TValue> TempSlice(Duration<TTime> duration) const
        {
            Duration<TTime> maxDuration = _tempBuffer.Data.Length / SliverSize;
            return Slice<TTime, TValue>(
                _tempBuffer,
                0,
                duration > maxDuration ? maxDuration : duration,
                SliverSize);
        }

        // Append the given amount of data.
        virtual void Append(Duration<TTime> duration, TValue* p)
        {
            Check(!IsShut);

            while (duration > 0)
            {
                EnsureFreeBuffer();

                // if source is larger than available free buffer, then we'll iterate
                Duration<TTime> durationToCopy(duration);
                if (durationToCopy > _remainingFreeSlice._duration)
                {
                    durationToCopy = _remainingFreeSlice._duration;
                }

                // now we know source can fit
                Slice<TTime, TValue> dest = _remainingFreeSlice.SubsliceOfDuration(source.Duration);
                dest.CopyFrom(p);

                // dest may well be adjacent to the previous slice, if there is one, since we may
                // be appending onto an open chunk.  So here is where we coalesce this, if so.
                dest = InternalAppend(dest);

                // and update our loop variables
                duration = duration - durationToCopy;

                Trim();
            }

            _discreteDuration = _discreteDuration + duration;
        }

        // Append this slice's data, by copying it into this stream's private buffers.
        virtual void Append(Slice<TTime, TValue> source)
        {
            Check(!IsShut);

            // Try to keep copying source into _remainingFreeSlice
            while (!source.IsEmpty())
            {
                EnsureFreeBuffer();

                // if source is larger than available free buffer, then we'll iterate
                Slice<TTime, TValue> originalSource = source;
                if (source.Duration > _remainingFreeSlice.Duration)
                {
                    source = source.Subslice(0, _remainingFreeSlice.Duration);
                }

                // now we know source can fit
                Slice<TTime, TValue> dest = _remainingFreeSlice.SubsliceOfDuration(source.Duration);
                source.CopyTo(dest);

                // dest may well be adjacent to the previous slice, if there is one, since we may
                // be appending onto an open chunk.  So here is where we coalesce this, if so.
                dest = InternalAppend(dest);

                // and update our loop variables
                source = originalSource.SubsliceStartingAt(source.Duration);

                Trim();
            }
        }

        // Internally append this slice (which must be allocated from our free buffer); this does the work
        // of coalescing, updating _data and other fields, etc.
        Slice<TTime, TValue> InternalAppend(Slice<TTime, TValue> dest)
        {
            Check(dest.Buffer.Data == _remainingFreeSlice.Buffer.Data); // dest must be from our free buffer

            if (_data.Count == 0)
            {
                _data.Add(new TimedSlice<TTime, TValue>(InitialTime, dest));
            }
            else
            {
                TimedSlice<TTime, TValue> last = _data[_data.Count - 1];
                if (last.Slice.Precedes(dest))
                {
                    _data[_data.Count - 1] = new TimedSlice<TTime, TValue>(last.InitialTime, last.Slice.UnionWith(dest));
                }
                else
                {
                    //Spam.Audio.WriteLine("BufferedSliceStream.InternalAppend: last did not precede; last slice is " + last.Slice + ", last slice time " + last.InitialTime + ", dest is " + dest);
                    _data.Add(new TimedSlice<TTime, TValue>(last.InitialTime + last.Slice.Duration, dest));
                }
            }

            _discreteDuration += _discreteDuration + dest.Duration;
            _remainingFreeSlice = _remainingFreeSlice.SubsliceStartingAt(dest.Duration);

            return dest;
        }

        // Copy strided data from a source array into a single destination sliver.
        void AppendSliver(TValue* source, int startOffset, int width, int stride, int height)
        {
            Check(source != null);
            int neededLength = startOffset + stride * (height - 1) + width;
            Check(source.Length >= neededLength);
            Check(SliverSize == width * height);
            Check(stride >= width);

            EnsureFreeBuffer();

            Slice<TTime, TValue> destination = _remainingFreeSlice.SubsliceOfDuration(1);

            int sourceOffset = startOffset;
            int destinationOffset = 0;
            for (int h = 0; h < height; h++)
            {
                destination.CopyFrom(source, sourceOffset, destinationOffset, width);

                sourceOffset += stride;
                destinationOffset += width;
            }

            InternalAppend(destination);

            Trim();
        }

        // Trim off any content beyond the maximum allowed to be buffered.
        void Trim()
        {
            if (_maxBufferedDuration == 0 || _discreteDuration <= _maxBufferedDuration)
            {
                return;
            }

            while (DiscreteDuration > _maxBufferedDuration)
            {
                Duration<TTime> toTrim = DiscreteDuration - _maxBufferedDuration;
                // get the first slice
                TimedSlice<TTime, TValue> firstSlice = _data[0];
                if (firstSlice.Slice.Duration <= toTrim)
                {
                    _data.RemoveAt(0);
#if DEBUG
                    for (TimedSlice<TTime, TValue> slice : _data) {
                        Check(slice.Slice.Buffer.Data != firstSlice.Slice.Buffer.Data,
                            "make sure our later stream data doesn't reference this one we're about to free");
                    }
#endif
                    _allocator->Free(firstSlice.Slice.Buffer);
                    _discreteDuration -= firstSlice.Slice.Duration;
                    _initialTime += firstSlice.Slice.Duration;
                }
                else
                {
                    TimedSlice<TTime, TValue> newFirstSlice = new TimedSlice<TTime, TValue>(
                        firstSlice.InitialTime + toTrim,
                        new Slice<TTime, TValue>(
                            firstSlice.Slice.Buffer,
                            firstSlice.Slice.Offset + toTrim,
                            firstSlice.Slice.Duration - toTrim,
                            SliverSize));
                    _data[0] = newFirstSlice;
                    _discreteDuration -= toTrim;
                    _initialTime += toTrim;
                }
            }
        }

        virtual void CopyTo(Interval<TTime> sourceInterval, TValue* p) const
        {
            while (!sourceInterval.IsEmpty)
            {
                Slice<TTime, TValue> source(GetNextSliceAt(sourceInterval));
                source.CopyTo(p);
                sourceInterval = sourceInterval.SubintervalStartingAt(source.Duration);
            }
        }

        virtual void CopyTo(Interval<TTime> sourceInterval, DenseSliceStream<TTime, TValue>* destinationStream) const
        {
            while (!sourceInterval.IsEmpty)
            {
                Slice<TTime, TValue> source = GetNextSliceAt(sourceInterval);
                destinationStream.Append(source);
                sourceInterval = sourceInterval.SubintervalStartingAt(source.Duration);
            }
        }

        // 
        // Map the interval time to stream local time, and get the next slice of it.
        // 
        // <param name="interval"></param>
        // <returns></returns>
        virtual Slice<TTime, TValue> GetNextSliceAt(Interval<TTime> interval) const
        {
            Interval<TTime> firstMappedInterval = _intervalMapper.MapNextSubInterval(interval);

            if (firstMappedInterval.IsEmpty)
            {
                return Slice<TTime, TValue>.Empty;
            }

            Check(firstMappedInterval.InitialTime >= InitialTime, "firstMappedInterval.InitialTime >= InitialTime");
            Check(firstMappedInterval.InitialTime + firstMappedInterval.Duration <= InitialTime + DiscreteDuration,
                "mapped interval fits within slice");

            TimedSlice<TTime, TValue> foundTimedSlice = GetInitialTimedSlice(firstMappedInterval);
            Interval<TTime> intersection = foundTimedSlice.Interval.Intersect(firstMappedInterval);
            Check(!intersection.IsEmpty, "interval intersects slice");
            Slice<TTime, TValue> ret = foundTimedSlice.Slice.Subslice(
                intersection.InitialTime - foundTimedSlice.InitialTime,
                intersection.Duration);

            return ret;
        }

        TimedSlice<TTime, TValue> GetInitialTimedSlice(Interval<TTime> firstMappedInterval) const
        {
            // we must overlap somewhere
            Check(!firstMappedInterval.Intersect(new Interval<TTime>(InitialTime, DiscreteDuration)).IsEmpty,
                "first mapped interval intersects this slice somewhere");

            // Get the biggest available slice at firstMappedInterval.InitialTime.
            // First, get the index of the slice just after the one we want.
            TimedSlice<TTime, TValue> target = new TimedSlice<TTime, TValue>(firstMappedInterval.InitialTime, Slice<TTime, TValue>.Empty);
            int originalIndex = _data.BinarySearch(target, TimedSlice<TTime, TValue>.Comparer.Instance);
            int index = originalIndex;

            if (index < 0)
            {
                // index is then the index of the next larger element
                // -- we know there is a smaller element because we know firstMappedInterval fits inside stream interval
                index = (~index) - 1;
                Check(index >= 0, "index >= 0");
            }

            TimedSlice<TTime, TValue> foundTimedSlice = _data[index];
            return foundTimedSlice;
        }
    };
}
