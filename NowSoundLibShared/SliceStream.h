// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"

#include <algorithm>

#include "BufferAllocator.h"
#include "Check.h"
#include "IStream.h"
#include "Slice.h"
#include "NowSoundTime.h"

namespace NowSound
{
    // A stream of data, which can be Shut, at which point it acquires a floating-point ContinuousDuration.
    // 
    // Streams may be Open (in which case more data may be appended to them), or Shut (in which case they will
    // not change again).
    // 
    // Streams have a SliverCount which denotes a larger granularity within the Stream's data.
    // A SliverCount of N represents that each element in the Stream logically consists of N contiguous
    // TValue entries in the stream's backing store; such a contiguous group is called a sliver.  
    // A Stream with duration 1 has exactly one sliver of data. 
    template<typename TTime, typename TValue>
    class SliceStream : public IStream<TTime>
    {
    protected:
        // The floating-point duration of this stream in terms of samples; only valid once shut.
        // 
        // This allows streams to have lengths measured in fractional samples, which prevents roundoff error from
        // causing clock drift when using unevenly divisible BPM values and looping for long periods.
        ContinuousDuration<TTime> _continuousDuration;

        // As with Slice<typeparam name="TValue"></typeparam>, this defines the number of T values in an
        // individual slice.
        int _sliverCount;

        // Is this stream shut?
        bool _isShut;

        SliceStream(int sliverCount, ContinuousDuration<AudioSample> continuousDuration, bool isShut)
            : _sliverCount{ sliverCount }, _continuousDuration{ continuousDuration }, _isShut{ isShut }
        {
        }

    public:
        virtual bool IsShut() const { return _isShut; }

        // The floating-point-accurate duration of this stream; only valid once shut.
        // This may have a fractional part if the BPM of the stream can't be evenly divided into
        // the sample rate.
        virtual ContinuousDuration<TTime> ExactDuration() const { return _continuousDuration; }

        // The number of T values in each sliver of this slice.
        // SliceDuration.Value() is the number of slivers in the slice;
        // the slice's size in bytes is SliceDuration.Value() * SliverCount() * sizeof(TValue).
        int SliverCount() const { return _sliverCount; }

        // Shut the stream; no further appends may be accepted.
        // 
        // finalDuration is the possibly fractional duration to be associated with the stream;
        // must be strictly equal to, or less than one sample smaller than, the discrete duration.
        virtual void Shut(ContinuousDuration<TTime> finalDuration)
        {
            Check(!IsShut());
            _isShut = true;
            _continuousDuration = finalDuration;
        }

        // Drop this stream and all its owned data.
        // 
        // This MAY need to become a ref counting structure if we want stream dependencies.
        virtual ~SliceStream() {};
    };

    // A stream of data, accessed through consecutive, densely sequenced Slices.
    // This type is mostly an interface.
    template<typename TTime, typename TValue>
    class DenseSliceStream : public SliceStream<TTime, TValue>
    {
    protected:
        // The discrete duration of this stream; always exactly equal to the sum of the durations of all contained slices,
        // and also exactly equal to the rounded-up value of ExactDuration().
        Duration<TTime> _discreteDuration;

        DenseSliceStream(
            int sliverCount,
            ContinuousDuration<TTime> exactDuration,
            bool isShut,
            Duration<TTime> discreteDuration)
            : SliceStream<TTime, TValue>(sliverCount, exactDuration, isShut),
            _discreteDuration{ discreteDuration }
        { }

    public:
        // The discrete duration of this stream; always exactly equal to the number of samples appended.
        virtual Duration<TTime> DiscreteDuration() const { return _discreteDuration; }

        Interval<TTime> DiscreteInterval() const { return Interval<TTime>(0, this->DiscreteDuration(), Direction::Forwards); }

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

        // Get a reference to the slice at the given time.
        // If there is no slice at the exact time, return a suffix of the preceding slice containing the start time.
        // If the next available slice is not as long as the source interval, return the largest available slice containing
        // the interval start time.
        // If the interval IsEmpty, return an empty slice.
        // If sourceInterval is Backwards, the resulting Slice will be the slice overlapping with that interval
        // in the backwards direction.
        virtual Slice<TTime, TValue> GetSliceIntersecting(Interval<TTime> sourceInterval) const = 0;

        // Append contiguous data.
        // This must not be shut yet.
        virtual void Append(const Slice<TTime, TValue>& source) = 0;

        // Append the given duration's worth of slices from the given pointer.
        // This must not be shut yet.
        virtual void Append(Duration<TTime> duration, const TValue* p) = 0;

        // Copy the given interval of this stream to the destination.
        virtual void CopyTo(const Interval<TTime>& sourceInterval, TValue* destination) const = 0;
    };

    // A stream that buffers some amount of data in memory.
    // This is an actual concrete implementation type.
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
        std::vector<OwningBuf<TValue>> _buffers;

        // This is the remaining not-yet-allocated portion of the current append buffer (the last in _buffers).
        Slice<TTime, TValue> _remainingFreeSlice;

        void EnsureFreeSlice()
        {
            if (_remainingFreeSlice.IsEmpty())
            {
                // allocate a new buffer and transfer ownership of it to _buffers
                _buffers.push_back(std::move(_allocator->Allocate()));

                // get a reference to the current append buffer
                OwningBuf<TValue>& appendBuffer = _buffers.at(_buffers.size() - 1);

                _remainingFreeSlice = Slice<TTime, TValue>(
                    Buf<TValue>(appendBuffer),
                    0,
                    appendBuffer.Length() / this->SliverCount(),
                    this->SliverCount());
            }
        }

        // Internally append this slice (which must be allocated from our free buffer); this does the work
        // of coalescing, updating _data and other fields, etc.
        void InternalAppend(const Slice<TTime, TValue>& source)
        {
            Check(source.Buffer().Data() == _remainingFreeSlice.Buffer().Data()); // dest must be from our free buffer

            if (_data.size() == 0)
            {
                _data.push_back(TimedSlice<TTime, TValue>(0, source));
            }
            else
            {
                TimedSlice<TTime, TValue> last = _data[_data.size() - 1];
                if (last.Value().Precedes(source))
                {
                    // coalesce the slices
                    _data[_data.size() - 1] = TimedSlice<TTime, TValue>(last.InitialTime(), last.Value().UnionWith(source));
                }
                else
                {
                    _data.push_back(TimedSlice<TTime, TValue>(last.InitialTime() + last.Value().SliceDuration(), source));
                }
            }

            this->_discreteDuration = this->_discreteDuration + source.SliceDuration();
            _remainingFreeSlice = _remainingFreeSlice.SubsliceStartingAt(source.SliceDuration());
        }

    public:
        BufferedSliceStream(
            int sliverCount,
            BufferAllocator<TValue>* allocator,
            Duration<TTime> maxBufferedDuration)
            : DenseSliceStream<TTime, TValue>(
                sliverCount,
                ContinuousDuration<TTime>{0},
                false, // isShut
                Duration<TTime>{}),
            _allocator{ allocator },
            _buffers{ },
            _remainingFreeSlice{ },
            _maxBufferedDuration{ maxBufferedDuration }
        { }

        BufferedSliceStream(
            int sliverCount,
            BufferAllocator<TValue>* allocator)
            : DenseSliceStream<TTime, TValue>(
                sliverCount,
                ContinuousDuration<TTime>{0},
                false, // isShut
                Duration<TTime>{}),
            _allocator{ allocator },
            _buffers{},
            _remainingFreeSlice{},
            _maxBufferedDuration{ Duration<TTime>{} }
        { }

        BufferedSliceStream(BufferedSliceStream<TTime, TValue>&& other)
            : DenseSliceStream<TTime, TValue>(
                other.SliverCount(),
                other.ExactDuration(),
                other.IsShut(),
                other.DiscreteDuration(),
                std::move(other._intervalMapper)),
            _allocator{ other._allocator },
            _buffers{ std::move(other._buffers) },
            _remainingFreeSlice{ other._remainingFreeSlice },
            _maxBufferedDuration{ other._maxBufferedDuration }
        {
            Check(_allocator != nullptr);
            Check(ExactDuration() == other.ExactDuration());
        }

        BufferedSliceStream(const BufferedSliceStream<TTime, TValue>& other) = delete;

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

        virtual void Shut(ContinuousDuration<AudioSample> finalDuration, bool fade)
        {
            this->DenseSliceStream<TTime, TValue>::Shut(finalDuration);

#if SPAMAUDIO
            foreach(TimedSlice<TTime, TValue> timedSlice in _data) {
                Spam.Audio.WriteLine("BufferedSliceStream.Shut: next slice time " + timedSlice.InitialTime + ", slice " + timedSlice.Slice);
            }
#endif

            if (fade)
            {
                // and, do a microfade out at the end of the last slice, and in at the start of the first.
                // this avoids clicking that was empirically otherwise present and annoying.
                // TODO: this should be dynamically done whenever loopie local time changes noncontinuously!
                const int64_t microFadeDuration{ 20 };
                TimedSlice<TTime, TValue>& firstSlice{ _data.at(0) };
                TValue* firstSliceData{ firstSlice.NonConstValue().OffsetPointer() };
                TimedSlice<TTime, TValue>& lastSlice{ _data.at(_data.size() - 1) };
                int sliverCount = firstSlice.Value().SliverCount();
                int64_t firstSliceDuration = firstSlice.Value().SliceDuration().Value();
                TValue* lastSliceDataEnd{ lastSlice.NonConstValue().OffsetPointer() + (lastSlice.Value().SliceDuration().Value() * sliverCount) };
                int64_t lastSliceDuration = lastSlice.Value().SliceDuration().Value();
                int64_t minSliceDuration = firstSliceDuration < lastSliceDuration ? firstSliceDuration : lastSliceDuration;
                int64_t minDuration = minSliceDuration < microFadeDuration ? minSliceDuration : microFadeDuration;

                for (int64_t i = 0; i < minDuration; i++)
                {
                    float frac = (float)i / minDuration;
                    for (int64_t j = 0; j < sliverCount; j++)
                    {
                        firstSliceData[i * sliverCount + j] *= frac;
                        lastSliceDataEnd[(-i - 1) * sliverCount + j] *= frac;
                    }
                }
            }
        }

        // Append the given amount of data.
        virtual void Append(Duration<TTime> duration, const TValue* p)
        {
            Check(!this->IsShut());

            while (duration > 0)
            {
                EnsureFreeSlice();

                // if source is larger than available free buffer, then we'll iterate
                Duration<TTime> durationToCopy(duration);
                if (durationToCopy > _remainingFreeSlice.SliceDuration())
                {
                    durationToCopy = _remainingFreeSlice.SliceDuration();
                }

                // now we know source can fit
                Slice<TTime, TValue> dest(_remainingFreeSlice.SubsliceOfDuration(durationToCopy));
                dest.CopyFrom(p);

                // dest may well be adjacent to the previous slice, if there is one, since we may
                // be appending onto an open chunk.  So here is where we coalesce this, if so.
                InternalAppend(dest);

                // and update our loop variables
                duration = duration - durationToCopy;

                Trim();
            }
        }

        // Append this slice's data, by copying it into this stream's private buffers.
        virtual void Append(const Slice<TTime, TValue>& sourceArgument)
        {
            Check(!this->IsShut());

            Slice<TTime, TValue> source = sourceArgument; // so it can be updated in the loop

            // Try to keep copying source into _remainingFreeSlice
            while (!source.IsEmpty())
            {
                EnsureFreeSlice();

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
                InternalAppend(dest);

                // and update our loop variables
                source = originalSource.SubsliceStartingAt(source.SliceDuration());

                Trim();
            }
        }

        // Copy strided data from a source array into a single destination sliver.
        void AppendSliver(TValue* source, int startOffset, int width, int stride, int height)
        {
            Check(source != nullptr);
            int neededLength = startOffset + stride * (height - 1) + width;
            Check(this->SliverCount() == width * height);
            Check(stride >= width);

            EnsureFreeSlice();

            Slice<TTime, TValue> destination = _remainingFreeSlice.SubsliceOfDuration(1);

            int sourceOffset = startOffset;
            int destinationOffset = 0;
            for (int h = 0; h < height; h++)
            {
                destination.CopyFrom(source + sourceOffset, destinationOffset, width);

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
                TimedSlice<TTime, TValue> firstSlice = _data[0];
                Duration<TTime> firstSliceDuration = firstSlice.Value().SliceDuration();
                if (firstSlice.Value().SliceDuration() <= toTrim)
                {
                    // the whole slice goes away
                    _data.erase(_data.begin());
#if DEBUG
                    for (TimedSlice<TTime, TValue> slice : _data) {
                        Check(slice.Slice.Buffer.Data != firstSlice.Slice.Buffer.Data,
                            "make sure our later stream data doesn't reference this one we're about to free");
                    }
#endif
                    Check(firstSlice.Value().Buffer().Data() == _buffers[0].Data());
                    _allocator->Free(std::move(_buffers.at(0)));
                    _buffers.erase(_buffers.begin());
                    this->_discreteDuration = this->_discreteDuration - firstSliceDuration;
                    // and adjust all remaining slices
                    for (int i = 0; i < _data.size(); i++) {
                        _data[i].ChangeInitialTimeBy(-firstSliceDuration.Value());
                    }
            }
                else
                {
                    // chop off the first part of the data
                    Slice<TTime, TValue> newSlice(
                        firstSlice.Value().Buffer(),
                        firstSlice.Value().Offset() + toTrim,
                        firstSlice.Value().SliceDuration() - toTrim,
                        this->SliverCount());
                    // initial time is 0 because this is the new first slice
                    TimedSlice<TTime, TValue> newFirstSlice(0, newSlice);
                    _data[0] = newFirstSlice;
                    this->_discreteDuration = this->_discreteDuration - toTrim;

                    // move all later slices back 
                    for (int i = 1; i < _data.size(); i++) {
                        _data[i].ChangeInitialTimeBy(-toTrim.Value());
                    }
                }
            }
        }

        // Copy the given interval's worth of data to the destination pointer.
        virtual void CopyTo(const Interval<TTime>& sourceIntervalArgument, TValue* p) const
        {
            // so we can update it in the loop
            Interval<TTime> sourceInterval = sourceIntervalArgument;
            while (!sourceInterval.IsEmpty())
            {
                Slice<TTime, TValue> source(GetSliceIntersecting(sourceInterval));
                source.CopyTo(p);
                sourceInterval = sourceInterval.Suffix(source.SliceDuration());
            }
        }

        // Append the given interval from this stream to the (end of the) destination stream.
        virtual void AppendTo(Interval<TTime> sourceInterval, DenseSliceStream<TTime, TValue>* destinationStream) const
        {
            while (!sourceInterval.IsEmpty())
            {
                Slice<TTime, TValue> source(GetSliceIntersecting(sourceInterval));
                destinationStream->Append(source);
                sourceInterval = sourceInterval.Suffix(source.SliceDuration());
            }
        }

        // Get the slice that starts at the interval's start time, and that is either
        // the longest available slice, or a slice no longer than the interval.
        virtual Slice<TTime, TValue> GetSliceIntersecting(Interval<TTime> interval) const
        {
            // What is the intersection

            if (interval.IsEmpty())
            {
                // default slice is empty (but has no backing buf at all)
                return Slice<TTime, TValue>::Empty();
            }

            if (this->DiscreteDuration() == 0)
            {
                // the stream is empty, there is no slice.
                // TODO: it would be better if this was an actual error... right?
                return Slice<TTime, TValue>::Empty();
            }

            const TimedSlice<TTime, TValue>& foundTimedSlice = GetFirstSliceIntersecting(interval);

            // TODO: make this handle backwards intervals (right now, will blow up here because intersecting
            // backwards interval with forwards interval, which results in interval with undefined direction)
            Interval<TTime> intersection = foundTimedSlice.SliceInterval().Intersect(interval);

            if (intersection.IsEmpty())
            {
                // no overlap
                // TODO: it would be better if this was an actual error... right?
                return Slice<TTime, TValue>::Empty();
            }
            else
            {
                Check(!intersection.IsEmpty());
                Slice<TTime, TValue> ret(foundTimedSlice.Value().Subslice(
                    intersection.IntervalTime() - foundTimedSlice.InitialTime(),
                    intersection.IntervalDuration()));

                return ret;
            }
        }

        // Get the first timed slice that contains data from this interval.
        const TimedSlice<TTime, TValue>& GetFirstSliceIntersecting(Interval<TTime> interval) const
        {
            // We must overlap somewhere; check for non-empty intersection.
            Interval<TTime> thisInterval = this->DiscreteInterval();
            Check(!interval.Intersect(thisInterval).IsEmpty());

            // Get the biggest available slice at or after interval.InitialTime.
            // First, get the index of the slice with start time that is at or after the interval's start time.
            TimedSlice<TTime, TValue> target(interval.IntervalTime(), Slice<TTime, TValue>());
            auto firstSliceEqualOrGreaterThanTarget = std::lower_bound(_data.begin(), _data.end(), target);

            if (firstSliceEqualOrGreaterThanTarget == _data.end())
            {
                // If we reached the end, then the slice we want is the last slice; it's less than the
                // start time of target, and it overlaps.
                return *(firstSliceEqualOrGreaterThanTarget - 1);
            }
            else if (firstSliceEqualOrGreaterThanTarget->InitialTime() == interval.IntervalTime())
            {
                // This is the exact right slice
                return *firstSliceEqualOrGreaterThanTarget;
            }
            else if (firstSliceEqualOrGreaterThanTarget != _data.begin())
            {
                // if there is a prior slice, return that
                return *(firstSliceEqualOrGreaterThanTarget - 1);
            }
            else
            {
                // return this slice, there's none earlier
                return *firstSliceEqualOrGreaterThanTarget;
            }
        }
    };
}
