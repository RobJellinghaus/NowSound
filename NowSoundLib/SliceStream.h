#pragma once

// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

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
    class SliceStream
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
        bool IsShut() { return _isShut; }

        // The starting time of this Stream.
        Time<TTime> InitialTime() { return _initialTime; }

        // The floating-point-accurate duration of this stream; only valid once shut.
        ContinuousDuration<AudioSample> ContinuousDuration() { return _continuousDuration; }

        // Shut the stream; no further appends may be accepted.
        // 
        // finalDuration is the possibly fractional duration to be associated with the stream;
        // must be strictly equal to, or less than one sample smaller than, the discrete duration.
        virtual void Shut(ContinuousDuration<AudioSample> finalDuration)
        {
            Check(!IsShut);
            _isShut = true;
            _continuousDuration = finalDuration;
        }

        // Drop this stream and all its owned data.
        // 
        // This MAY need to become a ref counting structure if we want stream dependencies.
        virtual void Dispose() = 0;
    };

    // A stream of data, accessed through consecutive, densely sequenced Slices.
    // 
    // The methods which take IntPtr arguments cannot be implemented generically in .NET; it is not possible to
    // take the address of a generic T[] array.  The type must be specialized to some known primitive type such
    // as float or byte.  This is done in the leaf subclasses in the Stream hierarchy.  All other operations are
    // generically implemented.
    // </remarks>
    template<typename TTime, typename TValue>
    class DenseSliceStream : SliceStream<TTime, TValue>
    {
        // The discrete duration of this stream; always exactly equal to the sum of the durations of all contained slices.
    protected:
        Duration<TTime> _discreteDuration;

        // The mapper that converts absolute time into relative time for this stream.
        IntervalMapper<TTime> _intervalMapper;

        DenseSliceStream(Time<TTime> initialTime, int sliverSize)
            : base(initialTime, sliverSize)
        {
        }

    public:
        // The discrete duration of this stream; always exactly equal to the number of timepoints appended.
        Duration<TTime> DiscreteDuration() { return _discreteDuration; }

        Interval<TTime> DiscreteInterval() { return new Interval<TTime>(InitialTime(), DiscreteDuration()); }

        IntervalMapper<TTime> IntervalMapper() { return _intervalMapper; }
        void IntervalMapper(IntervalMapper<TTime> value) { _intervalMapper = value; }

        // Shut the stream; no further appends may be accepted.
        // 
        // finalDuration is the possibly fractional duration to be associated with the stream;
        // must be strictly equal to, or less than one sample smaller than, the discrete duration.</param>
        virtual void Shut(ContinuousDuration<AudioSample> finalDuration)
        {
            Check(!IsShut);
            // Should always have as many samples as the rounded-up finalDuration.
            // The precise time matching behavior is that a loop will play either Math.Floor(finalDuration)
            // or Math.Ceiling(finalDuration) samples on each iteration, such that it remains perfectly in
            // time with finalDuration's fractional value.  So, a shut loop should have DiscreteDuration
            // equal to rounded-up ContinuousDuration.
            Check((int)Math.Ceiling((double)finalDuration) == (int)DiscreteDuration);
            base.Shut(finalDuration);
        }

        // Get a reference to the next slice at the given time, up to the given duration if possible, or the
        // largest available slice if not.
        // 
        // If the interval IsEmpty, return an empty slice.
        virtual Slice<TTime, TValue> GetNextSliceAt(Interval<TTime> sourceInterval) = 0;

        // Append contiguous data; this must not be shut yet.
        virtual void Append(Slice<TTime, TValue> source) = 0;

        // Append a rectangular, strided region of the source array.
        // 
        // The width * height must together equal the sliverSize.
        virtual void AppendSliver(TValue* source, int startOffset, int width, int stride, int height) = 0;

        // Append the given duration's worth of slices from the given pointer.
        virtual void Append(Duration<TTime> duration, TValue* p) = 0;

        // Copy the given interval of this stream to the destination.
        virtual void CopyTo(Interval<TTime> sourceInterval, DenseSliceStream<TTime, TValue> destination) = 0;

        // Copy the given interval of this stream to the destination.
        virtual void CopyTo(Interval<TTime> sourceInterval, TValue* destination) = 0;
    };

    // A stream that buffers some amount of data in memory.
    template<typename TTime, typename TValue>
    class BufferedSliceStream : DenseSliceStream<TTime, TValue>
    {
    private:
        // Allocator for buffer management.
        const BufferAllocator<TValue> _allocator;

        // The slices making up the buffered data itself.
        // The InitialTime of each entry in this list must exactly equal the InitialTime + Duration of the
        // previous entry; in other words, these are densely arranged in time.
        std::vector<TimedSlice<TTime, TValue>> _data{};

        // The maximum amount that this stream will buffer while it is open; more appends will cause
        // earlier data to be dropped.  If 0, no buffering limit will be enforced.
        Duration<TTime> _maxBufferedDuration;

        // Temporary space for, e.g., the IntPtr Append method.
        Buf<TValue> _tempBuffer; // = new Buf<TValue>(-1, new TValue[1024]); // -1 id = temp buf

        // This stream holds onto an entire buffer and copies data into it when appending.
        Slice<TTime, TValue> _remainingFreeBuffer;

        // The mapper that converts absolute time into relative time for this stream.
        IntervalMapper<TTime> _intervalMapper;

        bool _useContinuousLoopingMapper; // = false;

        public BufferedSliceStream(
            Time<TTime> initialTime,
            BufferAllocator<TValue> allocator,
            int sliverSize,
            Duration<TTime> maxBufferedDuration = default(Duration<TTime>),
            bool useContinuousLoopingMapper = false)
            : base(initialTime, sliverSize),
            _allocator(allocator),
            _maxBufferedDuration(maxBufferedDuration),
            _useContinuousLoopingMapper(useContinuousLoopingMapper)
        {
            // as long as we are appending, we use the identity mapping
            // TODO: support delay mapping
            _intervalMapper = new IdentityIntervalMapper<TTime, TValue>(this);
        }

        void EnsureFreeBuffer()
        {
            if (_remainingFreeBuffer.IsEmpty())
            {
                Buf<TValue> chunk = _allocator.Allocate();
                _remainingFreeBuffer = new Slice<TTime, TValue>(
                    chunk,
                    0,
                    (chunk.Data.Length / SliverSize),
                    SliverSize);
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
        Slice<TTime, TValue> TempSlice(Duration<TTime> duration)
        {
            Duration<TTime> maxDuration = _tempBuffer.Data.Length / SliverSize;
            return Slice<TTime, TValue>(
                _tempBuffer,
                0,
                duration > maxDuration ? maxDuration : duration,
                SliverSize);
        }

        // Append the given amount of data marshalled from the pointer P.
        virtual void Append(Duration<TTime> duration, TValue* p)
        {
            Check(!IsShut);

            while (duration > 0)
            {
                Slice<TTime, TValue> tempSlice = TempSlice(duration);

                TValue* src = p;
                TValue* dest = tempSlice.OffsetPointer();
                for (int i = 0; i < tempSlice._duration.Value() * _sliverSize; i++)
                {
                    *dest++ = *src++;
                }
                Append(tempSlice);
                duration = duration - tempSlice.Duration;
            }
            _discreteDuration = _discreteDuration + duration;
        }

        // Append this slice's data, by copying it into this stream's private buffers.
        virtual void Append(Slice<TTime, TValue> source)
        {
            Check(!IsShut);

            // Try to keep copying source into _remainingFreeBuffer
            while (!source.IsEmpty())
            {
                EnsureFreeBuffer();

                // if source is larger than available free buffer, then we'll iterate
                Slice<TTime, TValue> originalSource = source;
                if (source.Duration > _remainingFreeBuffer.Duration) {
                    source = source.Subslice(0, _remainingFreeBuffer.Duration);
                }

                // now we know source can fit
                Slice<TTime, TValue> dest = _remainingFreeBuffer.SubsliceOfDuration(source.Duration);
                source.CopyTo(dest);

                // dest may well be adjacent to the previous slice, if there is one, since we may
                // be appending onto an open chunk.  So here is where we coalesce this, if so.
                dest = InternalAppend(dest);

                // and update our loop variables
                source = originalSource.SubsliceStartingAt(source.Duration);

                Trim();
            }
        }

        // 
        // Internally append this slice (which must be allocated from our free buffer); this does the work
        // of coalescing, updating _data and other fields, etc.
        // 
        Slice<TTime, TValue> InternalAppend(Slice<TTime, TValue> dest)
        {
            Check(dest.Buffer.Data == _remainingFreeBuffer.Buffer.Data); // dest must be from our free buffer

            if (_data.Count == 0) {
                _data.Add(new TimedSlice<TTime, TValue>(InitialTime, dest));
            }
            else {
                TimedSlice<TTime, TValue> last = _data[_data.Count - 1];
                if (last.Slice.Precedes(dest)) {
                    _data[_data.Count - 1] = new TimedSlice<TTime, TValue>(last.InitialTime, last.Slice.UnionWith(dest));
                }
                else {
                    //Spam.Audio.WriteLine("BufferedSliceStream.InternalAppend: last did not precede; last slice is " + last.Slice + ", last slice time " + last.InitialTime + ", dest is " + dest);
                    _data.Add(new TimedSlice<TTime, TValue>(last.InitialTime + last.Slice.Duration, dest));
                }
            }

            _discreteDuration += _discreteDuration + dest.Duration;
            _remainingFreeBuffer = _remainingFreeBuffer.SubsliceStartingAt(dest.Duration);

            return dest;
        }

        // Copy strided data from a source array into a single destination sliver.
        virtual void AppendSliver(TValue[] source, int startOffset, int width, int stride, int height)
        {
            Check(source != null);
            int neededLength = startOffset + stride * (height - 1) + width;
            Check(source.Length >= neededLength);
            Check(SliverSize == width * height);
            Check(stride >= width);

            EnsureFreeBuffer();

            Slice<TTime, TValue> destination = _remainingFreeBuffer.SubsliceOfDuration(1);

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
            if (_maxBufferedDuration == 0 || _discreteDuration <= _maxBufferedDuration) {
                return;
            }

            while (DiscreteDuration > _maxBufferedDuration) {
                Duration<TTime> toTrim = DiscreteDuration - _maxBufferedDuration;
                // get the first slice
                TimedSlice<TTime, TValue> firstSlice = _data[0];
                if (firstSlice.Slice.Duration <= toTrim) {
                    _data.RemoveAt(0);
#if DEBUG
                    foreach(TimedSlice<TTime, TValue> slice in _data) {
                        Contract.Assert(slice.Slice.Buffer.Data != firstSlice.Slice.Buffer.Data,
                            "make sure our later stream data doesn't reference this one we're about to free");
                    }
#endif
                    _allocator.Free(firstSlice.Slice.Buffer);
                    _discreteDuration -= firstSlice.Slice.Duration;
                    _initialTime += firstSlice.Slice.Duration;
                }
                else {
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

        public override void CopyTo(Interval<TTime> sourceInterval, IntPtr p)
        {
            while (!sourceInterval.IsEmpty) {
                Slice<TTime, TValue> source = GetNextSliceAt(sourceInterval);
                _copySliceToIntPtrAction(source, p);
                sourceInterval = sourceInterval.SubintervalStartingAt(source.Duration);
            }
        }

        public override void CopyTo(Interval<TTime> sourceInterval, DenseSliceStream<TTime, TValue> destinationStream)
        {
            while (!sourceInterval.IsEmpty) {
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
        public override Slice<TTime, TValue> GetNextSliceAt(Interval<TTime> interval)
        {
            Interval<TTime> firstMappedInterval = _intervalMapper.MapNextSubInterval(interval);

            if (firstMappedInterval.IsEmpty) {
                return Slice<TTime, TValue>.Empty;
            }

            Contract.Assert(firstMappedInterval.InitialTime >= InitialTime, "firstMappedInterval.InitialTime >= InitialTime");
            Contract.Assert(firstMappedInterval.InitialTime + firstMappedInterval.Duration <= InitialTime + DiscreteDuration,
                "mapped interval fits within slice");

            TimedSlice<TTime, TValue> foundTimedSlice = GetInitialTimedSlice(firstMappedInterval);
            Interval<TTime> intersection = foundTimedSlice.Interval.Intersect(firstMappedInterval);
            Contract.Assert(!intersection.IsEmpty, "interval intersects slice");
            Slice<TTime, TValue> ret = foundTimedSlice.Slice.Subslice(
                intersection.InitialTime - foundTimedSlice.InitialTime,
                intersection.Duration);

            return ret;
        }

        TimedSlice<TTime, TValue> GetInitialTimedSlice(Interval<TTime> firstMappedInterval)
        {
            // we must overlap somewhere
            Contract.Requires(!firstMappedInterval.Intersect(new Interval<TTime>(InitialTime, DiscreteDuration)).IsEmpty,
                "first mapped interval intersects this slice somewhere");

            // Get the biggest available slice at firstMappedInterval.InitialTime.
            // First, get the index of the slice just after the one we want.
            TimedSlice<TTime, TValue> target = new TimedSlice<TTime, TValue>(firstMappedInterval.InitialTime, Slice<TTime, TValue>.Empty);
            int originalIndex = _data.BinarySearch(target, TimedSlice<TTime, TValue>.Comparer.Instance);
            int index = originalIndex;

            if (index < 0) {
                // index is then the index of the next larger element
                // -- we know there is a smaller element because we know firstMappedInterval fits inside stream interval
                index = (~index) - 1;
                Contract.Assert(index >= 0, "index >= 0");
            }

            TimedSlice<TTime, TValue> foundTimedSlice = _data[index];
            return foundTimedSlice;
        }

        public override void Dispose()
        {
            // release each T[] back to the buffer
            foreach(TimedSlice<TTime, TValue> slice in _data) {
                // this requires that Free be idempotent; in general we don't expect
                // many slices per buffer, since each Stream allocates from a private
                // buffer and coalesces aggressively
                _allocator.Free(slice.Slice.Buffer);
            }
        }
    }

    public static class SliceFloatExtension
    {
        // 
        // Copy data from IntPtr to Slice.
        // 
        public static void CopyToSlice<TTime>(this IntPtr src, Slice<TTime, float> dest)
        {
            Marshal.Copy(src, dest.Buffer.Data, (int)dest.Offset * dest.SliverSize, (int)dest.Duration * dest.SliverSize);
        }
        // 
        // Copy data from Slice to IntPtr.
        // 
        public static void CopyToIntPtr<TTime>(this Slice<TTime, float> src, IntPtr dest)
        {
            Marshal.Copy(src.Buffer.Data, (int)src.Offset * src.SliverSize, dest, (int)src.Duration * src.SliverSize);
        }
        // 
        // Invoke some underlying action with an IntPtr directly to a Slice's data.
        // 
        // 
        // The arguments to the action are an IntPtr to the fixed data, and the number of BYTES to act on.
        // </remarks>
        public static unsafe void RawAccess<TTime>(this Slice<TTime, float> src, Action<IntPtr, int> action)
        {
            // per http://www.un4seen.com/forum/?topic=12912.msg89978#msg89978
            fixed(float* p = &src.Buffer.Data[src.Offset * src.SliverSize]) {
                byte* b = (byte*)p;

                action(new IntPtr(p), (int)src.Duration * src.SliverSize * sizeof(float));
            }
        }
    }

    public static class SliceByteExtension
    {
        // 
        // Copy data from IntPtr to Slice.
        // 
        public static void CopyToSlice<TTime>(this IntPtr src, Slice<TTime, byte> dest)
        {
            Marshal.Copy(src, dest.Buffer.Data, (int)dest.Offset * dest.SliverSize, (int)dest.Duration * dest.SliverSize);
        }
        // 
        // Copy data from Slice to IntPtr.
        // 
        public static void CopyToIntPtr<TTime>(this Slice<TTime, byte> src, IntPtr dest)
        {
            Marshal.Copy(src.Buffer.Data, (int)src.Offset * src.SliverSize, dest, (int)src.Duration * src.SliverSize);
        }
        // 
        // Invoke some underlying action with an IntPtr directly to a Slice's data.
        // 
        // 
        // The action receives an IntPtr to the data, and an int that is a count of bytes.
        // </remarks>
        public static unsafe void RawAccess<TTime>(this Slice<TTime, byte> src, Action<IntPtr, int> action)
        {
            // per http://www.un4seen.com/forum/?topic=12912.msg89978#msg89978
            fixed(byte* p = &src.Buffer.Data[src.Offset * src.SliverSize]) {

                action(new IntPtr(p), (int)src.Duration * src.SliverSize);
            }
        }
    }

    public class DenseSampleFloatStream : BufferedSliceStream<AudioSample, float>
    {
        public DenseSampleFloatStream(
            Time<AudioSample> initialTime,
            BufferAllocator<float> allocator,
            int sliverSize,
            Duration<AudioSample> maxBufferedDuration = default(Duration<AudioSample>),
            bool useContinuousLoopingMapper = false)
            : base(initialTime,
                allocator,
                sliverSize,
                SliceFloatExtension.CopyToSlice,
                SliceFloatExtension.CopyToIntPtr,
                SliceFloatExtension.RawAccess,
                maxBufferedDuration,
                useContinuousLoopingMapper)
        {
        }

        public override int SizeofValue()
        {
            return sizeof(float);
        }
    }

    public class DenseFrameByteStream : BufferedSliceStream<Frame, byte>
    {
        public DenseFrameByteStream(
            BufferAllocator<byte> allocator,
            int sliverSize,
            Duration<Frame> maxBufferedDuration = default(Duration<Frame>),
            bool useContinuousLoopingMapper = false)
            : base(0,
                allocator,
                sliverSize,
                SliceByteExtension.CopyToSlice,
                SliceByteExtension.CopyToIntPtr,
                SliceByteExtension.RawAccess,
                maxBufferedDuration,
                useContinuousLoopingMapper)
        {
        }

        public override int SizeofValue()
        {
            return sizeof(byte);
        }
    }

    public class SparseSampleByteStream : SparseSliceStream<AudioSample, byte>
    {
        public SparseSampleByteStream(
            Time<AudioSample> initialTime,
            BufferAllocator<byte> allocator,
            int sliverSize,
            int maxBufferedFrameCount = 0)
            : base(initialTime,
                new DenseFrameByteStream(allocator, sliverSize, maxBufferedFrameCount),
                maxBufferedFrameCount)
        {
        }

        public override int SizeofValue()
        {
            return sizeof(byte);
        }
    }
    */
}
