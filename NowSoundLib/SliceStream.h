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
    // Streams have a SliverCount which denotes a larger granularity within the Stream's data.
    // A SliverCount of N represents that each element in the Stream logically consists of N contiguous
    // TValue entries in the stream's backing store; such a contiguous group is called a sliver.  
    // A Stream with duration 1 has exactly one sliver of data. 
    template<typename TTime, typename TValue>
    class SliceStream : public IStream<TTime>
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
        int _sliverCount;

        // Is this stream shut?
        bool _isShut;

        SliceStream(Time<TTime> initialTime, int sliverCount)
            : _initialTime(initialTime), _sliverCount(sliverCount), _continuousDuration{0}, _isShut{ false }
        {
        }

    public:
        virtual bool IsShut() const { return _isShut; }

        // The starting time of this Stream.
        virtual Time<TTime> InitialTime() const { return _initialTime; }

        // The floating-point-accurate duration of this stream; only valid once shut.
        // This may have a fractional part if the BPM of the stream can't be evenly divided into
        // the sample rate.
        virtual ContinuousDuration<AudioSample> ExactDuration() const { return _continuousDuration; }

        int SliverCount() const { return _sliverCount; }

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
        virtual ~SliceStream() = 0;
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
            : SliceStream<TTime, TValue>(initialTime, sliverSize),
            // when appending, we always start out with identity mapping
            _intervalMapper(new IdentityIntervalMapper<TTime>(this))
        {
        }

    public:
        // The discrete duration of this stream; always exactly equal to the number of timepoints appended.
        virtual Duration<TTime> DiscreteDuration() const { return _discreteDuration; }

        Interval<TTime> DiscreteInterval() const { return Interval<TTime>(this->InitialTime(), this->DiscreteDuration()); }

        const IntervalMapper<TTime>* Mapper() const { return _intervalMapper.get(); }
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
        BufferAllocator<TValue>* _allocator;

        // The slices making up the buffered data itself.
        // The InitialTime of each entry in this list must exactly equal the InitialTime + Duration of the
        // previous entry; in other words, these are densely arranged in time.
        // Note that slices borrow buffer references from their containing stream.
        std::vector<TimedSlice<TTime, TValue>> _data{};

        // The maximum amount that this stream will buffer while it is open; more appends will cause
        // earlier data to be dropped.  If 0, no buffering limit will be enforced.
        Duration<TTime> _maxBufferedDuration;

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

                // if _buffers every grows, all the un-owning Buf<T>* pointers in the stream's slices could break
                Check(_buffers.capacity() == DefaultBufferCount);

                // get a borrowed pointer to the current append buffer
                Buf<TValue>* appendBuffer = &_buffers.at(_buffers.size() - 1);

                _remainingFreeSlice = Slice<TTime, TValue>(
                    appendBuffer,
                    0,
                    appendBuffer->Length() / this->SliverCount(),
                    this->SliverCount());
            }
        }

        // Internally append this slice (which must be allocated from our free buffer); this does the work
        // of coalescing, updating _data and other fields, etc.
        Slice<TTime, TValue> InternalAppend(Slice<TTime, TValue> dest)
        {
            Check(dest.Buffer() == _remainingFreeSlice.Buffer()); // dest must be from our free buffer

            if (_data.size() == 0)
            {
                _data.push_back(TimedSlice<TTime, TValue>(this->InitialTime(), dest));
            }
            else
            {
                TimedSlice<TTime, TValue> last = _data[_data.size() - 1];
                if (last.Value().Precedes(dest))
                {
                    _data[_data.size() - 1] = TimedSlice<TTime, TValue>(last.InitialTime(), last.Value().UnionWith(dest));
                }
                else
                {
                    //Spam.Audio.WriteLine("BufferedSliceStream.InternalAppend: last did not precede; last slice is " + last.Slice + ", last slice time " + last.InitialTime + ", dest is " + dest);
                    _data.push_back(TimedSlice<TTime, TValue>(last.InitialTime() + last.Value().SliceDuration(), dest));
                }
            }

            this->_discreteDuration = this->_discreteDuration + dest.SliceDuration();
            _remainingFreeSlice = _remainingFreeSlice.SubsliceStartingAt(dest.SliceDuration());

            return dest;
        }

        // 256 1-second buffers is over a four-minute stream (assuming 1-second buffers).
        // TODO: really need a way for Slices to refer to their Bufs in a non-owning but stable-against-
        // backing-store-mutation way.
        static const int DefaultBufferCount = 256;

    public:
        BufferedSliceStream(
            Time<TTime> initialTime,
            BufferAllocator<TValue>* allocator,
            int sliverSize,
            Duration<TTime> maxBufferedDuration,
            bool useContinuousLoopingMapper)
            : DenseSliceStream<TTime, TValue>(initialTime, sliverSize),
            _allocator(allocator),
            _buffers(DefaultBufferCount),
            _remainingFreeSlice{},
            _maxBufferedDuration(maxBufferedDuration),
            _useContinuousLoopingMapper(useContinuousLoopingMapper)
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
            this->DenseSliceStream<TTime, TValue>::Shut(finalDuration);
            // swap out our mappers, we're looping now
            if (_useContinuousLoopingMapper)
            {
                this->_intervalMapper.reset(new LoopingIntervalMapper<TTime>(this));
            }
            else
            {
                this->_intervalMapper.reset(new SimpleLoopingIntervalMapper<TTime>(this));
            }

#if SPAMAUDIO
            foreach(TimedSlice<TTime, TValue> timedSlice in _data) {
                Spam.Audio.WriteLine("BufferedSliceStream.Shut: next slice time " + timedSlice.InitialTime + ", slice " + timedSlice.Slice);
            }
#endif
        }

        // Append the given amount of data.
        virtual void Append(Duration<TTime> duration, TValue* p)
        {
            Check(!this->IsShut());

            while (duration > 0)
            {
                EnsureFreeBuffer();

                // if source is larger than available free buffer, then we'll iterate
                Duration<TTime> durationToCopy(duration);
                if (durationToCopy > _remainingFreeSlice.SliceDuration())
                {
                    durationToCopy = _remainingFreeSlice.SliceDuration();
                }

                // now we know source can fit
                Slice<TTime, TValue> dest = _remainingFreeSlice.SubsliceOfDuration(duration);
                dest.CopyFrom(p);

                // dest may well be adjacent to the previous slice, if there is one, since we may
                // be appending onto an open chunk.  So here is where we coalesce this, if so.
                dest = InternalAppend(dest);

                // and update our loop variables
                duration = duration - durationToCopy;

                Trim();
            }

            this->_discreteDuration = this->_discreteDuration + duration;
        }

        // Append this slice's data, by copying it into this stream's private buffers.
        virtual void Append(Slice<TTime, TValue> source)
        {
            Check(!this->IsShut());

            // Try to keep copying source into _remainingFreeSlice
            while (!source.IsEmpty())
            {
                EnsureFreeBuffer();

                // if source is larger than available free buffer, then we'll iterate
                Slice<TTime, TValue> originalSource = source;
                if (source.SliceDuration() > _remainingFreeSlice.SliceDuration())
                {
                    source = source.Subslice(0, _remainingFreeSlice.SliceDuration());
                }

                // now we know source can fit
                Slice<TTime, TValue> dest = _remainingFreeSlice.SubsliceOfDuration(source.SliceDuration());
                source.CopyTo(dest);

                // dest may well be adjacent to the previous slice, if there is one, since we may
                // be appending onto an open chunk.  So here is where we coalesce this, if so.
                dest = InternalAppend(dest);

                // and update our loop variables
                source = originalSource.SubsliceStartingAt(source.SliceDuration());

                Trim();
            }
        }

        // Copy strided data from a source array into a single destination sliver.
        void AppendSliver(TValue* source, int startOffset, int width, int stride, int height)
        {
            Check(source != null);
            int neededLength = startOffset + stride * (height - 1) + width;
            Check(source.Length >= neededLength);
            Check(SliverCount == width * height);
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

        // Trim off any content from the earliest part of the stream beyond _maxBufferedDuration.
        void Trim()
        {
            if (_maxBufferedDuration == 0 || this->DiscreteDuration() <= _maxBufferedDuration)
            {
                return;
            }

            while (this->DiscreteDuration() > _maxBufferedDuration)
            {
                Duration<TTime> toTrim = this->DiscreteDuration() - _maxBufferedDuration;
                // get the first slice
                TimedSlice<TTime, TValue>& firstSlice = _data[0];
                if (firstSlice.Value().SliceDuration() <= toTrim)
                {
                    _data.erase(_data.begin());
#if DEBUG
                    for (TimedSlice<TTime, TValue> slice : _data) {
                        Check(slice.Slice.Buffer.Data != firstSlice.Slice.Buffer.Data,
                            "make sure our later stream data doesn't reference this one we're about to free");
                    }
#endif
                    Check(*(firstSlice.Value().Buffer()) == _buffers[0]);
                    _allocator->Free(std::move(_buffers.at(0)));
                    _buffers.erase(_buffers.begin());
                    this->_discreteDuration = this->_discreteDuration - firstSlice.Value().SliceDuration();
                    this->_initialTime = this->_initialTime + firstSlice.Value().SliceDuration();
                }
                else
                {
                    Slice<TTime, TValue> newSlice(
                        firstSlice.Value().Buffer(),
                        firstSlice.Value().Offset() + toTrim,
                        firstSlice.Value().SliceDuration() - toTrim,
                        this->SliverCount());
                    TimedSlice<TTime, TValue> newFirstSlice(
                        firstSlice.InitialTime() + toTrim,
                        newSlice);
                    _data[0] = newFirstSlice;
                    this->_discreteDuration = this->_discreteDuration - toTrim;
                    this->_initialTime = this->_initialTime - toTrim;
                }
            }
        }

        virtual void CopyTo(Interval<TTime> sourceInterval, TValue* p) const
        {
            while (!sourceInterval.IsEmpty())
            {
                Slice<TTime, TValue> source(GetNextSliceAt(sourceInterval));
                source.CopyTo(p);
                sourceInterval = sourceInterval.SubintervalStartingAt(source.SliceDuration());
            }
        }

        // Append the given interval from this stream to the (end of the) destination stream.
        virtual void AppendTo(Interval<TTime> sourceInterval, DenseSliceStream<TTime, TValue>* destinationStream) const
        {
            while (!sourceInterval.IsEmpty())
            {
                Slice<TTime, TValue> source = GetNextSliceAt(sourceInterval);
                destinationStream->Append(source);
                sourceInterval = sourceInterval.SubintervalStartingAt(source.SliceDuration());
            }
        }

        // Map the interval time to stream local time, and get the next slice of it.
        virtual Slice<TTime, TValue> GetNextSliceAt(Interval<TTime> interval) const
        {
            Interval<TTime> firstMappedInterval = this->Mapper()->MapNextSubInterval(interval);

            if (firstMappedInterval.IsEmpty())
            {
                // default slice is empty (but has no backing buf at all)
                return Slice<TTime, TValue>();
            }

            Check(firstMappedInterval.InitialTime() >= this->InitialTime());
            Check(firstMappedInterval.InitialTime() + firstMappedInterval.IntervalDuration() <= this->InitialTime() + this->DiscreteDuration());

            TimedSlice<TTime, TValue> foundTimedSlice = GetInitialTimedSlice(firstMappedInterval);
            Interval<TTime> intersection = foundTimedSlice.SliceInterval().Intersect(firstMappedInterval);
            Check(!intersection.IsEmpty());
            Slice<TTime, TValue> ret(foundTimedSlice.Value().Subslice(
                intersection.InitialTime() - foundTimedSlice.InitialTime(),
                intersection.IntervalDuration()));

            return ret;
        }

        // Get the first slice that intersects the given interval.
        TimedSlice<TTime, TValue> GetInitialTimedSlice(Interval<TTime> firstMappedInterval) const
        {
            // we must overlap somewhere
            Check(!firstMappedInterval.Intersect(this->DiscreteInterval()).IsEmpty());

            // Get the biggest available slice at firstMappedInterval.InitialTime.
            // First, get the index of the slice just after the one we want.
            TimedSlice<TTime, TValue> target(firstMappedInterval.InitialTime(), Slice<TTime, TValue>());
            int originalIndex = std::binary_search(_data.begin(), _data.end(), TimedSlice<TTime, TValue>::Compare);
            int index = originalIndex;

            if (index < 0)
            {
                // index is then the index of the next larger element
                // -- we know there is a smaller element because we know firstMappedInterval fits inside stream interval
                index = (~index) - 1;
                Check(index >= 0);
            }

            TimedSlice<TTime, TValue> foundTimedSlice = _data[index];
            return foundTimedSlice;
        }
    };
}
