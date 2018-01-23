// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"
#include "BufferAllocator.h"

using namespace NowSound;
using namespace std;
using namespace std::chrono;
using namespace winrt;

using namespace Windows::Foundation;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Render;

{
	/// <summary>
	/// Handle converting time intervals from absolute time (relative to start of app) to relative time
	/// (relative to start of loop).  
	/// </summary>
	/// <remarks>
	/// IntervalMappers are fundamentally how looping is implemented, by mapping current time modulo the
	/// loop duration.  They are also able to handle delaying, by mapping current time backwards within a rolling
	/// stream.
	/// </remarks>
	/// <typeparam name="TTime"></typeparam>
	abstract class IntervalMapper<TTime>
{
	/// <summary>
	/// Map an input Interval into a subset Interval.
	/// </summary>
	/// <remarks>
	/// This may return an Interval of shorter duration than the input; this is typically because
	/// the input interval wrapped around some underlying structure.  In this case, the function
	/// should be called again, with input.SubsliceStartingAt(returnedSubInterval.Duration) --
	/// in other words, slice off the portion that was mapped, and request the next portion.
	/// 
	/// The returned interval will have an initial time that is within the bounds of the stream
	/// it is mapping to.
	/// </remarks>
	public abstract Interval<TTime> MapNextSubInterval(Interval<TTime> input);
}

class IdentityIntervalMapper<TTime, TValue> : IntervalMapper<TTime>
	where TValue : struct
	{
		DenseSliceStream<TTime, TValue> m_stream;

		internal IdentityIntervalMapper(BufferedSliceStream<TTime, TValue> stream)
		{
			m_stream = stream;
		}

		public override Interval<TTime> MapNextSubInterval(Interval<TTime> input)
		{
			return input.Intersect(m_stream.DiscreteInterval);
		}
	}

	class SimpleLoopingIntervalMapper<TTime, TValue> : IntervalMapper<TTime>
		where TValue : struct
		{
			DenseSliceStream<TTime, TValue> m_stream;
			internal SimpleLoopingIntervalMapper(BufferedSliceStream<TTime, TValue> stream)
			{
				// Should only use this mapper on shut streams with a fixed ContinuousDuration.
				Debug.Assert(stream.IsShut);
				m_stream = stream;
			}

			public override Interval<TTime> MapNextSubInterval(Interval<TTime> input)
			{
				Contract.Requires(input.InitialTime >= m_stream.InitialTime, "input.InitialTime >= m_stream.InitialTime");

				Duration <TTime> inputDelayDuration = input.InitialTime - m_stream.InitialTime;
				// now we want to take that modulo the *discrete* duration
				inputDelayDuration %= m_stream.DiscreteDuration;
				Duration<TTime> mappedDuration = Math.Min((long)input.Duration, (long)(m_stream.DiscreteDuration - inputDelayDuration));
				Interval<TTime> ret = new Interval<TTime>(m_stream.InitialTime + inputDelayDuration, mappedDuration);

				Spam.Audio.WriteLine("SimpleLoopingIntervalMapper.MapNextSubInterval: m_stream " + m_stream + ", input " + input + ", ret " + ret);

				return ret;
			}
		}

		class LoopingIntervalMapper<TTime, TValue> : IntervalMapper<TTime>
			where TValue : struct
			{
				DenseSliceStream<TTime, TValue> m_stream;

				internal LoopingIntervalMapper(BufferedSliceStream<TTime, TValue> stream)
				{
					// Should only use this mapper on shut streams with a fixed ContinuousDuration.
					Debug.Assert(stream.IsShut);
					m_stream = stream;
				}

				public override Interval<TTime> MapNextSubInterval(Interval<TTime> input)
				{
					// for example reference
					/*
					LoopMult=AbsoluteTiem/ContinuousDuration
					LoopIndex=FLOOR(LoopMult,1)
					InitialTime=FLOOR(AbsoluteTime-(LoopIndex*ContinuousDuration),1)
					Duration=CEILING((LoopIndex+1)*ContinuousDuration-AbsoluteTime,1)

					FOr example, with ContinuousDuration 2.4:

					Absolute time   LoopMult	    LoopIndex   InitialTime	    Duration
					0	            0	            0	        0	            3
					1	            0.416666667	    0	        1           	2
					2	            0.833333333	    0	        2	            1
					3	            1.25	        1	        0	            2
					4	            1.666666667	    1	        1	            1
					5	            2.083333333	    2	        0	            3
					6	            2.5	            2	        1	            2
					7	            2.916666667	    2	        2	            1
					8	            3.333333333	    3	        0	            2
					9	            3.75	        3	        1	            1
					10	            4.166666667	    4	        0	            2
					11	            4.583333333	    4	        1	            1
					12	            5	            5	        0	            3
					13	            5.416666667	    5	        1	            2
					14	            5.833333333	    5	        2	            1
					15	            6.25	        6	        0	            2
					16	            6.666666667	    6	        1	            1
					17	            7.083333333	    7	        0	            3
					18	            7.5	            7           1	            2
					*/

					// First thing we do is, subtract our initial time from the initial time of the input.
					Duration<TTime> loopRelativeInitialTime = input.InitialTime - m_stream.InitialTime;
					float continuousDuration = (float)m_stream.ContinuousDuration;

					// Now, we need to figure out how many multiples of the stream's CONTINUOUS length this is.
					// In other words, we want adjustedInitialTime modulo the real-valued length of this stream.
					// This is critical to avoid iterated roundoff error with streams that are a multiple of a
					// fractional duration in length.
					float loopMult = ((float)loopRelativeInitialTime) / continuousDuration;
					int loopIndex = (int)loopMult;

					Duration<TTime> adjustedLoopRelativeInitialTime =
						(int)((float)loopRelativeInitialTime - (loopIndex * continuousDuration));

					int duration = (int)Math.Ceiling((loopIndex + 1) * continuousDuration - loopRelativeInitialTime);

					return new Interval<TTime>(m_stream.InitialTime + adjustedLoopRelativeInitialTime, duration);
				}
			}
}
