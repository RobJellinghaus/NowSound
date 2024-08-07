#include "stdafx.h"
#include "CppUnitTest.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "Histogram.h"
#include "Interval.h"
#include "Slice.h"
#include "SliceStream.h"
#include "NowSoundTime.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace NowSound;

namespace UnitTestsDesktop
{        
    TEST_CLASS(NowSoundDesktopTests)
    {
    public:        
        TEST_METHOD(TestCheck)
        {
            // this should be fine
            Check(true);
        }

        TEST_METHOD(TestHistogram)
        {
            Histogram h(4);
            h.Add(10);
            Check(h.Min() == 10);
            Check(h.Max() == 10);
            Check(h.Average() == 10);
            h.Add(20);
            Check(h.Min() == 10);
            Check(h.Max() == 20);
            Check(h.Average() == 15);
            h.Add(30);
            Check(h.Min() == 10);
            Check(h.Max() == 30);
            Check(h.Average() == 20);
            h.Add(0);
            Check(h.Min() == 0);
            Check(h.Max() == 30);
            Check(h.Average() == 15);
            h.Add(-10);
            Check(h.Min() == -10);
            Check(h.Max() == 30);
            Check(h.Average() == 10);
            h.Add(-20);
            Check(h.Min() == -20);
            Check(h.Max() == 30);
            Check(h.Average() == 0);
            h.Add(-30);
            Check(h.Min() == -30);
            Check(h.Max() == 0);
            Check(h.Average() == -15);
        }

        static const int FloatSliceSize = 2;
        static const int FloatNumSlices = 128;

        // Exercise minimal buffer allocation and free list reuse.
        TEST_METHOD(TestBufferAllocator)
        {
            BufferAllocator<float> bufferAllocator(FloatNumSlices * 2048, 1);
            OwningBuf<float> f(bufferAllocator.Allocate());
            Check(f.Length() == FloatSliceSize * 1024 * FloatNumSlices);

            OwningBuf<float> f2(bufferAllocator.Allocate());
            Check(f.Length() == f2.Length());
            // this is technically unsafe but we want to check free list reuse
            float* f2ptr = f2.Data();
            bufferAllocator.Free(std::move(f2));
            OwningBuf<float> f3 = bufferAllocator.Allocate();
            Check(f2ptr == f3.Data()); // need to pull from free list first
        }

        // Fill a slice with simple linear data.
        static void PopulateFloatSlice(Slice<AudioSample, float> slice)
        {
            Check(slice.SliceSize() == 2);
            for (int64_t i = 0; i < slice.SliceDuration().Value(); i++) {
                slice.Get(i, 0) = (float)i;
                slice.Get(i, 1) = (float)i + 0.5f;
            }
        }

        // Verify data from PopulateFloatSlice.
        static void VerifySlice(Slice<AudioSample, float> slice)
        {
            Check(slice.SliceSize() == 2);
            for (int64_t i = 0; i < slice.SliceDuration().Value(); i++) {
                Check(slice.Get(i, 0) == i);
                Check(slice.Get(i, 1) == i + 0.5);
            }
        }

        // Test slice allocation, data insertion and access, and subslicing.
        TEST_METHOD(TestSlice)
        {
            BufferAllocator<float> bufferAllocator(FloatNumSlices * 2048, 1);
            OwningBuf<float> buffer(bufferAllocator.Allocate());
            Slice<AudioSample, float> slice(Buf<float>(buffer), 0, FloatNumSlices, FloatSliceSize);
            Check(slice.SliceDuration() == FloatNumSlices);
            Check(slice.IsEmpty() == false);
            Check(slice.SliceSize() == FloatSliceSize);
            int halfDuration = (FloatNumSlices / 2);
            Slice<AudioSample, float> prefixSlice = slice.Subslice(0, halfDuration);
            Slice<AudioSample, float> prefixSlice2 = slice.SubsliceOfDuration(halfDuration);
            Slice<AudioSample, float> suffixSlice = slice.Subslice(halfDuration, halfDuration);
            Slice<AudioSample, float> suffixSlice2 = slice.SubsliceStartingAt(halfDuration);
            Check(prefixSlice.Precedes(suffixSlice));
            Check(prefixSlice.Precedes(suffixSlice2));
            Check(prefixSlice2.Precedes(suffixSlice));
            Check(prefixSlice2.Precedes(suffixSlice2));

            PopulateFloatSlice(slice);
            VerifySlice(slice);

            OwningBuf<float> buffer2(bufferAllocator.Allocate());
            Slice<AudioSample, float> slice2(Buf<float>(buffer2), 0, FloatNumSlices, FloatSliceSize);

            slice.CopyTo(slice2);

            VerifySlice(slice);
            VerifySlice(slice2);
        }
        
        /// Simple basic stream test: make one, append two slices to it, ensure they get merged.
        TEST_METHOD(TestStream)
        {
            BufferAllocator<float> bufferAllocator(FloatNumSlices * 2048, 1);
            
            BufferedSliceStream<AudioSample, float> stream(FloatSliceSize, &bufferAllocator);

            Check(stream.DiscreteDuration() == 0);

            Interval<AudioSample> interval(0, 10, Direction::Forwards);
            Slice<AudioSample, float> firstSlice(stream.GetSliceIntersecting(interval));
            Check(firstSlice.IsEmpty());

            // Now let's fill a float array...
            int length = FloatNumSlices * FloatSliceSize;
            OwningBuf<float> owningBuf(-1, length);
            Duration<AudioSample> floatNumSlicesDuration = FloatNumSlices;
            Slice<AudioSample, float> tempSlice(Buf<float>(owningBuf), FloatSliceSize);
            PopulateFloatSlice(tempSlice);

            // now append in chunks
            stream.Append(tempSlice.SubsliceOfDuration(tempSlice.SliceDuration() / 2));
            stream.Append(tempSlice.SubsliceStartingAt(tempSlice.SliceDuration() / 2));

            Check(stream.DiscreteDuration() == FloatNumSlices);

            Slice<AudioSample, float> theSlice = stream.GetSliceIntersecting(stream.DiscreteInterval());

            VerifySlice(theSlice);
            Check(theSlice.SliceDuration() == floatNumSlicesDuration);
        }

        static float Verify4SliceFloatStream(const BufferedSliceStream<AudioSample, float>& stream, float f)
        {
            Interval<AudioSample> interval = stream.DiscreteInterval();
            while (!interval.IsEmpty()) {
                Slice<AudioSample, float> nextSlice = stream.GetSliceIntersecting(interval);
                for (int i = 0; i < nextSlice.SliceDuration().Value(); i++) {
                    Check(nextSlice.Get(i, 0) == f);
                    Check(nextSlice.Get(i, 1) == f + 0.25f);
                    Check(nextSlice.Get(i, 2) == f + 0.5f);
                    Check(nextSlice.Get(i, 3) == f + 0.75f);
                    f++;
                }
                interval = interval.Suffix(nextSlice.SliceDuration());
            }
            return f;
        }

        static float* AllocateSmall4FloatArray(int numSlices)
        {
            float* tinyBuffer = new float[numSlices * 4];
            float f = 0;
            for (int i = 0; i < numSlices; i++) {
                tinyBuffer[i * 4] = f;
                tinyBuffer[i * 4 + 1] = f + 0.25f;
                tinyBuffer[i * 4 + 2] = f + 0.5f;
                tinyBuffer[i * 4 + 3] = f + 0.75f;
                f++;
            }
            return tinyBuffer;
        }

        TEST_METHOD(TestStreamChunky)
        {
            const int sliceSize = 4; // 4 floats = 16 bytes
            const int sliceCount = 11; // 11 slices per buffer, to test various cases
            const int biggestChunk = 5; // max size of slice to copy in middle loop
            BufferAllocator<float> bufferAllocator(sliceSize * sliceCount, 1);

            BufferedSliceStream<AudioSample, float> stream(sliceSize, &bufferAllocator);

            Check(stream.DiscreteDuration() == 0);

            float f = 0;
            int chunkSliceSize = biggestChunk * sliceSize;
            OwningBuf<float> owningBuf(-2, chunkSliceSize);
            float* tinyBuffer = owningBuf.Data();
            for (int i = 0; i < 100; i++) {
                for (int c = 1; c <= 5; c++) {
                    for (int j = 0; j < c; j++) {
                        tinyBuffer[j * sliceSize] = f;
                        tinyBuffer[j * sliceSize + 1] = f + 0.25f;
                        tinyBuffer[j * sliceSize + 2] = f + 0.5f;
                        tinyBuffer[j * sliceSize + 3] = f + 0.75f;
                        f++;
                    }
                    Slice<AudioSample, float> tempSlice(Buf<float>(owningBuf), 0, c, sliceSize);
                    stream.Append(tempSlice);
                }
            }

            BufferAllocator<float> bigBufferAllocator(sliceSize * 1024, 1);
            BufferedSliceStream<AudioSample, float> bigStream(sliceSize, &bigBufferAllocator);

            stream.AppendTo(stream.DiscreteInterval(), &bigStream);

            Check(Verify4SliceFloatStream(stream, 0) == 1500);
            Check(Verify4SliceFloatStream(bigStream, 0) == 1500);

            BufferedSliceStream<AudioSample, float> stream2(sliceSize, &bufferAllocator);
            bigStream.AppendTo(bigStream.DiscreteInterval(), &stream2);

            Check(Verify4SliceFloatStream(stream2, 0) == 1500);
        }

        TEST_METHOD(TestStreamAppending)
        {
            const int sliceSize = 4; // 4 floats = 16 bytes
            const int sliceCount = 11; // 11 slices per buffer, to test various cases
            int bufferLength = sliceCount * sliceSize;
            BufferAllocator<float> bufferAllocator(bufferLength, 1);

            float* buffer = AllocateSmall4FloatArray(sliceCount);
            OwningBuf<float> owningBuf(0, bufferLength, buffer);

            BufferedSliceStream<AudioSample, float> stream(sliceSize, &bufferAllocator);

            stream.Append(Duration<AudioSample>(sliceCount), buffer);

            Check(stream.DiscreteDuration() == sliceCount);

            Check(Verify4SliceFloatStream(stream, 0) == 11);

            // clear original buffer to test copying back into it
            for (int i = 0; i < bufferLength; i++) {
                buffer[i] = 0;
            }

            stream.CopyTo(stream.DiscreteInterval(), buffer);

            BufferedSliceStream<AudioSample, float> stream2(sliceSize, &bufferAllocator);
            stream2.Append(Slice<AudioSample, float>(Buf<float>(owningBuf), sliceSize));

            Check(Verify4SliceFloatStream(stream2, 0) == 11);
        }

        // Test getting slices of stream data using forwards intervals (the norm).
        // Note that in comments we write a forwards interval with initial time 5 and duration 7 as [5, 7)
        // to denote that it is a closed-open interval.
        TEST_METHOD(TestStreamSlicing)
        {
            const int sliceSize = 4; // 4 floats = 16 bytes
            const int sliceCount = 11; // 11 slices per buffer, to test various cases
            int bufferLength = sliceCount * sliceSize;
            BufferAllocator<float> bufferAllocator(bufferLength, 1);

            float* buffer = AllocateSmall4FloatArray(sliceCount * 2);
            OwningBuf<float> owningBuf(0, bufferLength * 2, buffer);

            BufferedSliceStream<AudioSample, float> stream(sliceSize, &bufferAllocator);
            stream.Append(Slice<AudioSample, float>(Buf<float>(owningBuf), sliceSize));
            Check(stream.DiscreteDuration().Value() == 22);

            // test getting slices from existing stream
            // should return slice with duration 2, because [-2, 2) intersected with [0, 22) = [0, 2)
            Slice<AudioSample, float> beforeFirst = stream.GetSliceIntersecting(Interval<AudioSample>((-2), 4, Direction::Forwards));
            Check(beforeFirst.SliceDuration() == 2);

            // should return slice with duration 3, because [19, 24) intersected with [0, 22) = [19, 22)
            Slice<AudioSample, float> afterLast = stream.GetSliceIntersecting(Interval<AudioSample>(19, 5, Direction::Forwards));
            Check(afterLast.SliceDuration() == 3);

            // now get slice at [7, 8); result is [7, 4) because stream splits at time 11
            Interval<AudioSample> splitInterval(7, 8, Direction::Forwards);
            Slice<AudioSample, float> beforeSplit = stream.GetSliceIntersecting(splitInterval);
            Check(beforeSplit.Offset() == 7);
            Check(beforeSplit.SliceDuration() == 4);

            // Now get [11, 4)
            Interval<AudioSample> afterBufferSplitInterval(11, 4, Direction::Forwards);
            Slice<AudioSample, float> afterSplit = stream.GetSliceIntersecting(afterBufferSplitInterval);
            Check(afterSplit.Offset() == 0);
            Check(afterSplit.SliceDuration() == beforeSplit.SliceDuration());
            float lastBefore = beforeSplit.Get(3, 0);
            float firstAfter = afterSplit.Get(0, 0);
            Check(lastBefore + 1 == firstAfter);

            float* testStrideCopy = new float[12]{
                0, 0, 1, 1, 0, 0,
                0, 0, 2, 2, 0, 0,
            };

            stream.AppendStridedData(testStrideCopy, 2, 2, 6, 2);

            Slice<AudioSample, float> lastIndividualSlice = stream.GetSliceIntersecting(Interval<AudioSample>(22, 1, Direction::Forwards));
            Check(lastIndividualSlice.SliceDuration() == 1);
            Check(lastIndividualSlice.Get(0, 0) == 1);
            Check(lastIndividualSlice.Get(0, 1) == 1);
            Check(lastIndividualSlice.Get(0, 2) == 2);
            Check(lastIndividualSlice.Get(0, 3) == 2);

            Slice<AudioSample, float> firstSlice = stream.GetSliceIntersecting(Interval<AudioSample>(-2, 100, Direction::Forwards));
            Check(firstSlice.SliceDuration() == 11);
        }

        // Test getting slices of stream data using backwards intervals (rare but fun).
        // Note that in comments we write a backwards interval with initial time 5 and duration 7 as [<5, 7)
        // to both show that the interval is actually for times less than 5, and to visually "point backwards"
        // (given English left-to-right directionality).
        TEST_METHOD(TestBackwardsStreamSlicing)
        {
            const int sliceSize = 4; // 4 floats = 16 bytes
            const int sliceCount = 11; // 11 slices per buffer, to test various cases
            int bufferLength = sliceCount * sliceSize;
            BufferAllocator<float> bufferAllocator(bufferLength, 1);

            float* buffer = AllocateSmall4FloatArray(sliceCount * 2);
            OwningBuf<float> owningBuf(0, bufferLength * 2, buffer);

            BufferedSliceStream<AudioSample, float> stream(sliceSize, &bufferAllocator);
            stream.Append(Slice<AudioSample, float>(Buf<float>(owningBuf), sliceSize));
            Check(stream.DiscreteDuration().Value() == 22);

            // test that interval before start time of stream doesn't overlap
            Interval<AudioSample> backwards((-2), 4, Direction::Backwards);
            Check(stream.DiscreteInterval().Intersect(backwards).IsEmpty());

            // test getting slice from [<2, 5) (should be [0, 2) )
            Slice<AudioSample, float> beforeFirst = stream.GetSliceIntersecting(Interval<AudioSample>(2, 5, Direction::Backwards));
            Check(!beforeFirst.IsEmpty());
            Check(beforeFirst.Offset().Value() == 0);
            Check(beforeFirst.SliceDuration().Value() == 2);

            // should return slice with duration 3, because [<24, 5) intersected with [0, 22) (should) = [19, 22)
            Slice<AudioSample, float> afterLast = stream.GetSliceIntersecting(Interval<AudioSample>(24, 5, Direction::Backwards));
            Check(afterLast.SliceDuration() == 3);

            // now get slice across the buffer boundary (at time 11); verify it is split as expected
            // this effectively intersects [11, 11) with [<15, 8)
            Interval<AudioSample> splitInterval(15, 8, Direction::Backwards);
            Slice<AudioSample, float> afterSplit = stream.GetSliceIntersecting(splitInterval);
            Check(afterSplit.Offset() == 0);
            Check(afterSplit.SliceDuration() == 4);

            // Now get [<11, 4)
            Interval<AudioSample> beforeBufferSplitInterval(11, 4, Direction::Backwards);
            Slice<AudioSample, float> beforeSplit = stream.GetSliceIntersecting(beforeBufferSplitInterval);
            Check(beforeSplit.Offset() == 7);
            Check(afterSplit.SliceDuration() == beforeSplit.SliceDuration());
            float lastBefore = beforeSplit.Get(3, 0);
            float firstAfter = afterSplit.Get(0, 0);
            Check(lastBefore + 1 == firstAfter);

            /* TODO: what is all this anyway, add comments in original then fix this up 
            * 
            float* testStrideCopy = new float[12]{
                0, 0, 1, 1, 0, 0,
                0, 0, 2, 2, 0, 0,
            };

            stream.AppendStridedData(testStrideCopy, 2, 2, 6, 2);

            Slice<AudioSample, float> lastIndividualSlice = stream.GetSliceIntersecting(Interval<AudioSample>(22, 1, Direction::Forwards));
            Check(lastIndividualSlice.SliceDuration() == 1);
            Check(lastIndividualSlice.Get(0, 0) == 1);
            Check(lastIndividualSlice.Get(0, 1) == 1);
            Check(lastIndividualSlice.Get(0, 2) == 2);
            Check(lastIndividualSlice.Get(0, 3) == 2);

            Slice<AudioSample, float> firstSlice = stream.GetSliceIntersecting(Interval<AudioSample>(-2, 100, Direction::Forwards));
            Check(firstSlice.SliceDuration() == 11);
            */
        }

        TEST_METHOD(TestStreamShutting)
        {
            const int sliceSize = 4; // 4 floats = 16 bytes
            const int sliceCount = 11; // 11 slices per buffer, to test various cases
            BufferAllocator<float> bufferAllocator(sliceSize * sliceCount, 1);

            float continuousDuration = 2.4f;
            int discreteDuration = (int)std::ceil(continuousDuration);
            float* buffer = AllocateSmall4FloatArray(discreteDuration);
            OwningBuf<float> owningBuf(0, discreteDuration * sliceSize, buffer);
            BufferedSliceStream<AudioSample, float> stream(sliceSize, &bufferAllocator, 0);
            stream.Append(Slice<AudioSample, float>(Buf<float>(owningBuf), sliceSize));

            // OK, time to get this fractional business right, to ensure we properly handle loops that are
            // not an exact number of samples.
            stream.Shut(continuousDuration, /* fade: */false);
            Check(stream.IsShut());

            Interval<AudioSample> interval(0, 10, Direction::Forwards);
            Slice<AudioSample, float> slice = stream.GetSliceIntersecting(interval);
            Check(slice.SliceDuration() == 3);
            Check(slice.Get(0, 0) == 0);
            Check(slice.Get(2, 0) == 2);

            BufferedSliceStream<AudioSample, float> stream2(sliceSize, &bufferAllocator, 0);
            stream2.Append(Slice<AudioSample, float>(Buf<float>(owningBuf), sliceSize));
            stream2.Shut(continuousDuration, /* fade: */false);
            interval = Interval<AudioSample>(0, 10, Direction::Forwards);
            slice = stream2.GetSliceIntersecting(interval);
            Check(slice.SliceDuration() == 3);
            Check(slice.Get(0, 0) == 0);
            Check(slice.Get(2, 0) == 2);
        }

        TEST_METHOD(TestStreamTruncating)
        {
            const int sliceSize = 4; // 4 floats = 16 bytes
            const int sliceCount = 11; // 11 slices per buffer, to test various cases
            BufferAllocator<float> bufferAllocator(sliceSize * sliceCount, 1);

            ContinuousDuration<AudioSample> continuousDuration{ 12.4f };
            Duration<AudioSample> discreteDuration = continuousDuration.RoundedUp();
            float* buffer = AllocateSmall4FloatArray(discreteDuration.Value());
            OwningBuf<float> owningBuf(0, discreteDuration.Value() * sliceSize, buffer);
            BufferedSliceStream<AudioSample, float> stream(sliceSize, &bufferAllocator, 0);
            stream.Append(Slice<AudioSample, float>(Buf<float>(owningBuf), sliceSize));

            // Confirm we have two actual buffers in the stream.
            Check(stream.BufferCount() == 2);
            
            // Confirm that the two buffers are split start to end.
            Interval<AudioSample> startInterval(0, 1, Direction::Forwards);
            Slice<AudioSample, float> startSlice = stream.GetSliceIntersecting(startInterval);
            Interval<AudioSample> endInterval(discreteDuration.Value() - 1, 1, Direction::Forwards);
            Slice<AudioSample, float> endSlice = stream.GetSliceIntersecting(endInterval);
            Check(startSlice.Buffer().Data() != endSlice.Buffer().Data());

            // Truncate exactly one slice.
            // Should result in a stream with continuousDuration of 11.4, and still the
            // same two buffers.
            Duration<AudioSample> truncatedDuration{ discreteDuration - Duration<AudioSample>{ 1 } };
            stream.Truncate(truncatedDuration);

            Check(stream.DiscreteDuration() == truncatedDuration);

            Interval<AudioSample> endInterval2(truncatedDuration.Value() - 1, 1, Direction::Forwards);
            Slice<AudioSample, float> endSlice2 = stream.GetSliceIntersecting(endInterval2);
            Check(endSlice.Buffer().Data() == endSlice2.Buffer().Data());

            // OK, now truncate such that we have to drop a buffer.
            Duration<AudioSample> truncatedDuration2{ 5 };
            stream.Truncate(truncatedDuration2);

            Check(stream.DiscreteDuration() == truncatedDuration2);
            Check(stream.BufferCount() == 1);
        }

        /* TODO: perhaps revive this test? I think I already have coverage of Free(), so postponing porting this.
        [TestMethod]
        public void TestDispose()
        {
            const int sliceSize = 4; // 4 floats = 16 bytes
            const int sliceCount = 11; // 11 slices per buffer, to test various cases
            BufferAllocator<float> bufferAllocator = new BufferAllocator<float>(sliceSize * sliceCount, 1);

            float continuousDuration = 2.4f;
            int discreteDuration = (int)Math.Round(continuousDuration + 1);
            float[] tempBuffer = AllocateSmall4FloatArray(discreteDuration, sliceSize);

            // check that allocated, then freed, buffers are used first for next allocation
            Buf<float> buffer = bufferAllocator.Allocate();
            bufferAllocator.Free(buffer);
            Buf<float> buffer2 = bufferAllocator.Allocate();
            Check(buffer.Data == buffer2.Data);

            // free it again so stream can get it
            bufferAllocator.Free(buffer);

            BufferedSliceStream<AudioSample, float> stream = new BufferedSliceStream<AudioSample, float>(0, bufferAllocator, sliceSize);
            stream.Append(new Slice<AudioSample, float>(new Buf<float>(-6, tempBuffer), sliceSize));

            Verify4SliceFloatStream(stream, 0);

            // have stream drop it; should free buffer
            stream.Dispose();

            // make sure we get it back again
            buffer2 = bufferAllocator.Allocate();
            Check(buffer.Data == buffer2.Data);
        }
        */

        TEST_METHOD(TestLimitedBufferingStream)
        {
            const int sliceSize = 4; // 4 floats = 16 bytes
            const int sliceCount = 11; // 11 slices per buffer, to test various cases
            BufferAllocator<float> bufferAllocator(sliceSize * sliceCount, 1);

            float* tempBuffer = AllocateSmall4FloatArray(20);
            OwningBuf<float> owningBuf(0, 20 * sliceSize, tempBuffer);

            BufferedSliceStream<AudioSample, float> stream(sliceSize, &bufferAllocator, /*maxBufferedDuration:*/ 5);
            stream.Append(Slice<AudioSample, float>(Buf<float>(owningBuf), 0, 11, sliceSize));
            Check(stream.DiscreteDuration() == 5);
            Slice<AudioSample, float> slice = stream.GetSliceIntersecting(stream.DiscreteInterval());
            Check(slice.Get(0, 0) == 6);

            stream.Append(Slice<AudioSample, float>(Buf<float>(owningBuf), 11, 5, sliceSize));
            Check(stream.DiscreteDuration() == 5);
            slice = stream.GetSliceIntersecting(stream.DiscreteInterval());
            Check(slice.Get(0, 0) == 11);
        }

        /*
        [TestMethod]
        public void TestSparseSampleByteStream()
        {
            const int sliceSize = 2 * 2 * 4; // uncompressed 2x2 RGBA image... worst case
            const int bufferSliceCount = 10;
            BufferAllocator<byte> allocator = new BufferAllocator<byte>(sliceSize * bufferSliceCount, 1);

            byte[] appendBuffer = new byte[sliceSize];
            for (int i = 0; i < sliceSize; i++) {
                appendBuffer[i] = (byte)i;
            }

            SparseSampleByteStream stream = new SparseSampleByteStream(10, allocator, sliceSize);
            stream.Append(11, new Slice<Frame, byte>(new Buf<byte>(-9, appendBuffer), sliceSize));

            // now let's get it back out
            Slice<Frame, byte> slice = stream.GetClosestSlice(11);
            Check(slice.SliceDuration() == 1);
            Check(slice.SliceSize() == sliceSize);
            for (int i = 0; i < sliceSize; i++) {
                Check(slice.Get(0, i) == (byte)i);
            }

            // now let's copy it to intptr
            byte[] target = new byte[sliceSize];
            unsafe{
                fixed(byte* p = target) {
                IntPtr pp = new IntPtr(p);
                stream.CopyTo(11, pp);
            }
            }

            for (int i = 0; i < sliceSize; i++) {
                Check(target[i) == (byte)i);
            }

            SparseSampleByteStream stream2 = new SparseSampleByteStream(10, allocator, sliceSize);
            unsafe{
                fixed(byte* p = target) {
                IntPtr pp = new IntPtr(p);
                stream2.Append(11, pp);
            }
            }

            Slice<Frame, byte> slice2 = stream2.GetClosestSlice(12);
            Check(slice2.SliceDuration() == 1);
            Check(slice2.SliceSize() == sliceSize);
            for (int i = 0; i < sliceSize; i++) {
                Check(slice2.Get(0, i) == (byte)i);
            }

            // now verify looping and shutting work as expected
            for (int i = 0; i < appendBuffer.Length; i++) {
                appendBuffer[i] += (byte)appendBuffer.Length;
            }
            stream2.Append(21, new Slice<Frame, byte>(new Buf<byte>(-10, appendBuffer), sliceSize));

            Slice<Frame, byte> slice3 = stream2.GetClosestSlice(12);
            Check(slice3.SliceDuration() == 1);
            Check(slice3.SliceSize() == sliceSize);
            Check(slice3.Get(0, 0) == (byte)0);
            Slice<Frame, byte> slice4 = stream2.GetClosestSlice(22);
            Check(slice4.SliceDuration() == 1);
            Check(slice4.SliceSize() == sliceSize);
            Check(slice4.Get(0, 0) == (byte)sliceSize);

            stream2.Shut((ContinuousDuration)20);

            // now the closest slice to 32 should be the first slice
            Slice<Frame, byte> slice5 = stream2.GetClosestSlice(32);
            Check(slice5.SliceDuration() == 1);
            Check(slice5.SliceSize() == sliceSize);
            Check(slice5.Get(0, 0) == (byte)0);
            // and 42, the second
            Slice<Frame, byte> slice6 = stream2.GetClosestSlice(42);
            Check(slice6.SliceDuration() == 1);
            Check(slice6.SliceSize() == sliceSize);
            Check(slice6.Get(0, 0) == (byte)sliceSize);
        }
        */
    };
}