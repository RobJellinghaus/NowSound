#include "stdafx.h"
#include "CppUnitTest.h"

#include "BufferAllocator.h"
#include "Check.h"
#include "Histogram.h"
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

        static const int FloatSliverCount = 2;
        static const int FloatNumSlices = 128;

        // Exercise minimal buffer allocation and free list reuse.
        TEST_METHOD(TestBufferAllocator)
        {
            BufferAllocator<float> bufferAllocator(FloatNumSlices * 2048, 1);
            OwningBuf<float> f(bufferAllocator.Allocate());
            Check(f.Length() == FloatSliverCount * 1024 * FloatNumSlices);

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
            Check(slice.SliverCount() == 2);
            for (int64_t i = 0; i < slice.SliceDuration().Value(); i++) {
                slice.Get(i, 0) = (float)i;
                slice.Get(i, 1) = (float)i + 0.5f;
            }
        }

        // Verify data from PopulateFloatSlice.
        static void VerifySlice(Slice<AudioSample, float> slice)
        {
            Check(slice.SliverCount() == 2);
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
            Slice<AudioSample, float> slice(Buf<float>(buffer), 0, FloatNumSlices, FloatSliverCount);
            Check(slice.SliceDuration() == FloatNumSlices);
            Check(slice.IsEmpty() == false);
            Check(slice.SliverCount() == FloatSliverCount);
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
            Slice<AudioSample, float> slice2(Buf<float>(buffer2), 0, FloatNumSlices, FloatSliverCount);

            slice.CopyTo(slice2);

            VerifySlice(slice);
            VerifySlice(slice2);
        }
        
        /// Simple basic stream test: make one, append two slices to it, ensure they get merged.
        TEST_METHOD(TestStream)
        {
            BufferAllocator<float> bufferAllocator(FloatNumSlices * 2048, 1);
            
            BufferedSliceStream<AudioSample, float> stream(FloatSliverCount, &bufferAllocator);

            Check(stream.DiscreteDuration() == 0);

            Interval<AudioSample> interval(0, 10);
            Slice<AudioSample, float> firstSlice(stream.GetSliceContaining(interval));
            Check(firstSlice.IsEmpty());

            // Now let's fill a float array...
            int length = FloatNumSlices * FloatSliverCount;
            OwningBuf<float> owningBuf(-1, length);
            Duration<AudioSample> floatNumSlicesDuration = FloatNumSlices;
            Slice<AudioSample, float> tempSlice(Buf<float>(owningBuf), FloatSliverCount);
            PopulateFloatSlice(tempSlice);

            // now append in chunks
            stream.Append(tempSlice.SubsliceOfDuration(tempSlice.SliceDuration() / 2));
            stream.Append(tempSlice.SubsliceStartingAt(tempSlice.SliceDuration() / 2));

            Check(stream.InitialTime() == 0);
            Check(stream.DiscreteDuration() == FloatNumSlices);

            Slice<AudioSample, float> theSlice = stream.GetSliceContaining(stream.DiscreteInterval());

            VerifySlice(theSlice);
            Check(theSlice.SliceDuration() == floatNumSlicesDuration);
        }

        static float Verify4SliceFloatStream(const BufferedSliceStream<AudioSample, float>& stream, float f)
        {
            Interval<AudioSample> interval = stream.DiscreteInterval();
            while (!interval.IsEmpty()) {
                Slice<AudioSample, float> nextSlice = stream.GetSliceContaining(interval);
                for (int i = 0; i < nextSlice.SliceDuration().Value(); i++) {
                    Check(nextSlice.Get(i, 0) == f);
                    Check(nextSlice.Get(i, 1) == f + 0.25f);
                    Check(nextSlice.Get(i, 2) == f + 0.5f);
                    Check(nextSlice.Get(i, 3) == f + 0.75f);
                    f++;
                }
                interval = interval.SubintervalStartingAt(nextSlice.SliceDuration());
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
            const int sliverCount = 4; // 4 floats = 16 bytes
            const int sliceCount = 11; // 11 slices per buffer, to test various cases
            const int biggestChunk = 5; // max size of slice to copy in middle loop
            BufferAllocator<float> bufferAllocator(sliverCount * sliceCount, 1);

            BufferedSliceStream<AudioSample, float> stream(sliverCount, &bufferAllocator);

            Check(stream.DiscreteDuration() == 0);

            float f = 0;
            int chunkSliverCount = biggestChunk * sliverCount;
            OwningBuf<float> owningBuf(-2, chunkSliverCount);
            float* tinyBuffer = owningBuf.Data();
            for (int i = 0; i < 100; i++) {
                for (int c = 1; c <= 5; c++) {
                    for (int j = 0; j < c; j++) {
                        tinyBuffer[j * sliverCount] = f;
                        tinyBuffer[j * sliverCount + 1] = f + 0.25f;
                        tinyBuffer[j * sliverCount + 2] = f + 0.5f;
                        tinyBuffer[j * sliverCount + 3] = f + 0.75f;
                        f++;
                    }
                    Slice<AudioSample, float> tempSlice(Buf<float>(owningBuf), 0, c, sliverCount);
                    stream.Append(tempSlice);
                }
            }

            BufferAllocator<float> bigBufferAllocator(sliverCount * 1024, 1);
            BufferedSliceStream<AudioSample, float> bigStream(sliverCount, &bigBufferAllocator);

            stream.AppendTo(stream.DiscreteInterval(), &bigStream);

            Check(Verify4SliceFloatStream(stream, 0) == 1500);
            Check(Verify4SliceFloatStream(bigStream, 0) == 1500);

            BufferedSliceStream<AudioSample, float> stream2(sliverCount, &bufferAllocator);
            bigStream.AppendTo(bigStream.DiscreteInterval(), &stream2);

            Check(Verify4SliceFloatStream(stream2, 0) == 1500);
        }

        TEST_METHOD(TestStreamAppending)
        {
            const int sliverCount = 4; // 4 floats = 16 bytes
            const int sliceCount = 11; // 11 slices per buffer, to test various cases
            int bufferLength = sliceCount * sliverCount;
            BufferAllocator<float> bufferAllocator(bufferLength, 1);

            float* buffer = AllocateSmall4FloatArray(sliceCount);
            OwningBuf<float> owningBuf(0, bufferLength, buffer);

            BufferedSliceStream<AudioSample, float> stream(sliverCount, &bufferAllocator);

            stream.Append(Duration<AudioSample>(sliceCount), buffer);

            Check(stream.DiscreteDuration() == sliceCount);

            Check(Verify4SliceFloatStream(stream, 0) == 11);

            // clear original buffer to test copying back into it
            for (int i = 0; i < bufferLength; i++) {
                buffer[i] = 0;
            }

            stream.CopyTo(stream.DiscreteInterval(), buffer);

            BufferedSliceStream<AudioSample, float> stream2(sliverCount, &bufferAllocator);
            stream2.Append(Slice<AudioSample, float>(Buf<float>(owningBuf), sliverCount));

            Check(Verify4SliceFloatStream(stream2, 0) == 11);
        }

        TEST_METHOD(TestStreamSlicing)
        {
            const int sliverCount = 4; // 4 floats = 16 bytes
            const int sliceCount = 11; // 11 slices per buffer, to test various cases
            int bufferLength = sliceCount * sliverCount;
            BufferAllocator<float> bufferAllocator(bufferLength, 1);

            float* buffer = AllocateSmall4FloatArray(sliceCount * 2);
            OwningBuf<float> owningBuf(0, bufferLength * 2, buffer);

            BufferedSliceStream<AudioSample, float> stream(sliverCount, &bufferAllocator);
            stream.Append(Slice<AudioSample, float>(Buf<float>(owningBuf), sliverCount));
            Check(stream.DiscreteDuration().Value() == 22);

            // test getting slices from existing stream
            // should return slice with duration 2, because [-2, 2) intersected with [0, 22) = [0, 2)
            Slice<AudioSample, float> beforeFirst = stream.GetSliceContaining(Interval<AudioSample>((-2), 4));
            Check(beforeFirst.SliceDuration() == 2);

            // should return slice with duration 3, because [19, 24) intersected with [0, 22) = [19, 22)
            Slice<AudioSample, float> afterLast = stream.GetSliceContaining(Interval<AudioSample>(19, 5));
            Check(afterLast.SliceDuration() == 3);

            // now get slice across the buffer boundary (at time 11); verify it is split as expected
            Interval<AudioSample> splitInterval(7, 8);
            Slice<AudioSample, float> beforeSplit = stream.GetSliceContaining(splitInterval);
            Check(beforeSplit.Offset() == 7);
            Check(beforeSplit.SliceDuration() == 4);

            // Now get [11, 15)
            Interval<AudioSample> afterBufferSplitInterval = splitInterval.SubintervalStartingAt(beforeSplit.SliceDuration());
            Slice<AudioSample, float> afterSplit = stream.GetSliceContaining(afterBufferSplitInterval);
            Check(afterSplit.Offset() == 0);
            Check(afterSplit.SliceDuration() == beforeSplit.SliceDuration());
            float lastBefore = beforeSplit.Get(3, 0);
            float firstAfter = afterSplit.Get(0, 0);
            Check(lastBefore + 1 == firstAfter);

            float* testStrideCopy = new float[12] {
                0, 0, 1, 1, 0, 0,
                0, 0, 2, 2, 0, 0,
            };
            
            stream.AppendSliver(testStrideCopy, 2, 2, 6, 2);

            Slice<AudioSample, float> lastSliver = stream.GetSliceContaining(Interval<AudioSample>(22, 1));
            Check(lastSliver.SliceDuration() == 1);
            Check(lastSliver.Get(0, 0) == 1);
            Check(lastSliver.Get(0, 1) == 1);
            Check(lastSliver.Get(0, 2) == 2);
            Check(lastSliver.Get(0, 3) == 2);

            Slice<AudioSample, float> firstSlice = stream.GetSliceContaining(Interval<AudioSample>(-2, 100));
            Check(firstSlice.SliceDuration() == 11);
        }

        TEST_METHOD(TestStreamShutting)
        {
            const int sliverCount = 4; // 4 floats = 16 bytes
            const int sliceCount = 11; // 11 slices per buffer, to test various cases
            BufferAllocator<float> bufferAllocator(sliverCount * sliceCount, 1);

            float continuousDuration = 2.4f;
            int discreteDuration = (int)std::floor(continuousDuration + 1);
            float* buffer = AllocateSmall4FloatArray(discreteDuration);
            OwningBuf<float> owningBuf(0, discreteDuration * sliverCount, buffer);
            BufferedSliceStream<AudioSample, float> stream(0, sliverCount, &bufferAllocator, 0, /*useExactLoopingMapper:*/true);
            stream.Append(Slice<AudioSample, float>(Buf<float>(owningBuf), sliverCount));

            // OK, time to get this fractional business right, to ensure we properly handle loops that are
            // not an exact number of samples.
            stream.Shut(continuousDuration);
            Check(stream.IsShut());

            // now test looping
            Interval<AudioSample> interval(0, 10);
            // we expect this to be [0, 1, 2, 0, 1, 0, 1, 2, 0, 1]
            // or rather, [0>3], [0>2], [0>3], [0>2]
            Slice<AudioSample, float> slice = stream.GetSliceContaining(interval);
            Check(slice.SliceDuration() == 3);
            Check(slice.Get(0, 0) == 0);
            Check(slice.Get(2, 0) == 2);

            interval = interval.SubintervalStartingAt(slice.SliceDuration());
            slice = stream.GetSliceContaining(interval);
            Check(slice.SliceDuration() == 2);
            Check(slice.Get(0, 0) == 0);
            Check(slice.Get(1, 0) == 1);

            interval = interval.SubintervalStartingAt(slice.SliceDuration());
            slice = stream.GetSliceContaining(interval);
            Check(slice.SliceDuration() == 3);
            Check(slice.Get(0, 0) == 0);
            Check(slice.Get(2, 0) == 2);

            interval = interval.SubintervalStartingAt(slice.SliceDuration());
            slice = stream.GetSliceContaining(interval);
            Check(slice.SliceDuration() == 2);
            Check(slice.Get(0, 0) == 0);
            Check(slice.Get(1, 0) == 1);

            interval = interval.SubintervalStartingAt(slice.SliceDuration());
            Check(interval.IsEmpty());

            BufferedSliceStream<AudioSample, float> stream2(0, sliverCount, &bufferAllocator, 0, /*useContinuousLoopingMapper:*/ false);
            stream2.Append(Slice<AudioSample, float>(Buf<float>(owningBuf), sliverCount));
            stream2.Shut(continuousDuration);
            interval = Interval<AudioSample>(0, 10);
            slice = stream2.GetSliceContaining(interval);
            Check(slice.SliceDuration() == 3);
            Check(slice.Get(0, 0) == 0);
            Check(slice.Get(2, 0) == 2);

            interval = interval.SubintervalStartingAt(slice.SliceDuration());
            slice = stream2.GetSliceContaining(interval);
            Check(slice.SliceDuration() == 3);
            Check(slice.Get(0, 0) == 0);
            Check(slice.Get(1, 0) == 1);

            interval = interval.SubintervalStartingAt(slice.SliceDuration());
            slice = stream2.GetSliceContaining(interval);
            Check(slice.SliceDuration() == 3);
            Check(slice.Get(0, 0) == 0);
            Check(slice.Get(2, 0) == 2);

            interval = interval.SubintervalStartingAt(slice.SliceDuration());
            slice = stream2.GetSliceContaining(interval);
            Check(slice.SliceDuration() == 1);
            Check(slice.Get(0, 0) == 0);
        }

        /* TODO: perhaps revive this test? I think I already have coverage of Free(), so postponing porting this.
        [TestMethod]
        public void TestDispose()
        {
            const int sliverCount = 4; // 4 floats = 16 bytes
            const int sliceCount = 11; // 11 slices per buffer, to test various cases
            BufferAllocator<float> bufferAllocator = new BufferAllocator<float>(sliverCount * sliceCount, 1);

            float continuousDuration = 2.4f;
            int discreteDuration = (int)Math.Round(continuousDuration + 1);
            float[] tempBuffer = AllocateSmall4FloatArray(discreteDuration, sliverCount);

            // check that allocated, then freed, buffers are used first for next allocation
            Buf<float> buffer = bufferAllocator.Allocate();
            bufferAllocator.Free(buffer);
            Buf<float> buffer2 = bufferAllocator.Allocate();
            Check(buffer.Data == buffer2.Data);

            // free it again so stream can get it
            bufferAllocator.Free(buffer);

            BufferedSliceStream<AudioSample, float> stream = new BufferedSliceStream<AudioSample, float>(0, bufferAllocator, sliverCount);
            stream.Append(new Slice<AudioSample, float>(new Buf<float>(-6, tempBuffer), sliverCount));

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
            const int sliverCount = 4; // 4 floats = 16 bytes
            const int sliceCount = 11; // 11 slices per buffer, to test various cases
            BufferAllocator<float> bufferAllocator(sliverCount * sliceCount, 1);

            float* tempBuffer = AllocateSmall4FloatArray(20);
            OwningBuf<float> owningBuf(0, 20 * sliverCount, tempBuffer);

            BufferedSliceStream<AudioSample, float> stream(0, sliverCount, &bufferAllocator, /*maxBufferedDuration:*/ 5, /*useContinuousLoopingMapper:*/ false);
            stream.Append(Slice<AudioSample, float>(Buf<float>(owningBuf), 0, 11, sliverCount));
            Check(stream.DiscreteDuration() == 5);
            Slice<AudioSample, float> slice = stream.GetSliceContaining(stream.DiscreteInterval());
            Check(slice.Get(0, 0) == 6);

            stream.Append(Slice<AudioSample, float>(Buf<float>(owningBuf), 11, 5, sliverCount));
            Check(stream.DiscreteDuration() == 5);
            Check(stream.InitialTime() == 11);
            slice = stream.GetSliceContaining(stream.DiscreteInterval());
            Check(slice.Get(0, 0) == 11);
        }

        /*
        [TestMethod]
        public void TestSparseSampleByteStream()
        {
            const int sliverCount = 2 * 2 * 4; // uncompressed 2x2 RGBA image... worst case
            const int bufferSlivers = 10;
            BufferAllocator<byte> allocator = new BufferAllocator<byte>(sliverCount * bufferSlivers, 1);

            byte[] appendBuffer = new byte[sliverCount];
            for (int i = 0; i < sliverCount; i++) {
                appendBuffer[i] = (byte)i;
            }

            SparseSampleByteStream stream = new SparseSampleByteStream(10, allocator, sliverCount);
            stream.Append(11, new Slice<Frame, byte>(new Buf<byte>(-9, appendBuffer), sliverCount));

            // now let's get it back out
            Slice<Frame, byte> slice = stream.GetClosestSliver(11);
            Check(slice.SliceDuration() == 1);
            Check(slice.SliverCount() == sliverCount);
            for (int i = 0; i < sliverCount; i++) {
                Check(slice.Get(0, i) == (byte)i);
            }

            // now let's copy it to intptr
            byte[] target = new byte[sliverCount];
            unsafe{
                fixed(byte* p = target) {
                IntPtr pp = new IntPtr(p);
                stream.CopyTo(11, pp);
            }
            }

            for (int i = 0; i < sliverCount; i++) {
                Check(target[i) == (byte)i);
            }

            SparseSampleByteStream stream2 = new SparseSampleByteStream(10, allocator, sliverCount);
            unsafe{
                fixed(byte* p = target) {
                IntPtr pp = new IntPtr(p);
                stream2.Append(11, pp);
            }
            }

            Slice<Frame, byte> slice2 = stream2.GetClosestSliver(12);
            Check(slice2.SliceDuration() == 1);
            Check(slice2.SliverCount() == sliverCount);
            for (int i = 0; i < sliverCount; i++) {
                Check(slice2.Get(0, i) == (byte)i);
            }

            // now verify looping and shutting work as expected
            for (int i = 0; i < appendBuffer.Length; i++) {
                appendBuffer[i] += (byte)appendBuffer.Length;
            }
            stream2.Append(21, new Slice<Frame, byte>(new Buf<byte>(-10, appendBuffer), sliverCount));

            Slice<Frame, byte> slice3 = stream2.GetClosestSliver(12);
            Check(slice3.SliceDuration() == 1);
            Check(slice3.SliverCount() == sliverCount);
            Check(slice3.Get(0, 0) == (byte)0);
            Slice<Frame, byte> slice4 = stream2.GetClosestSliver(22);
            Check(slice4.SliceDuration() == 1);
            Check(slice4.SliverCount() == sliverCount);
            Check(slice4.Get(0, 0) == (byte)sliverCount);

            stream2.Shut((ContinuousDuration)20);

            // now the closest sliver to 32 should be the first sliver
            Slice<Frame, byte> slice5 = stream2.GetClosestSliver(32);
            Check(slice5.SliceDuration() == 1);
            Check(slice5.SliverCount() == sliverCount);
            Check(slice5.Get(0, 0) == (byte)0);
            // and 42, the second
            Slice<Frame, byte> slice6 = stream2.GetClosestSliver(42);
            Check(slice6.SliceDuration() == 1);
            Check(slice6.SliverCount() == sliverCount);
            Check(slice6.Get(0, 0) == (byte)sliverCount);
        }
        */
    };
}