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
	/// A reference to a sub-segment of an underlying buffer, indexed by the given TTime type.
	/// </summary>
	public struct Slice<TTime, TValue>
	where TValue : struct
	{
		/// <summary>
		/// The backing store; logically divided into slivers.
		/// </summary>
		readonly Buf<TValue> m_buffer;

		/// <summary>
		/// The number of slivers contained.
		/// </summary>
		public readonly Duration<TTime> Duration;

		/// <summary>
		/// The index to the sliver at which this slice actually begins.
		/// </summary>
		readonly Duration<TTime> m_offset;

		/// <summary>
		/// The size of each sliver in this slice; a count of T.
		/// </summary>
		/// <remarks>
		/// Slices are composed of multiple Slivers, one per unit of Duration.
		/// </remarks>
		public readonly int SliverSize;

		public Slice(Buf<TValue> buffer, Duration<TTime> offset, Duration<TTime> duration, int sliverSize)
		{
			Contract.Requires(buffer.Data != null);
			Contract.Requires(offset >= 0);
			Contract.Requires(duration >= 0);
			Contract.Requires((offset * sliverSize) + (duration * sliverSize) <= buffer.Data.Length);

			m_buffer = buffer;
			m_offset = offset;
			Duration = duration;
			SliverSize = sliverSize;
		}

		public Slice(Buf<TValue> buffer, int sliverSize)
			: this(buffer, 0, (buffer.Data.Length / sliverSize), sliverSize)
		{
		}

		static Buf<TValue> s_emptyBuf = new Buf<TValue>(0, new TValue[0]);

		public static Slice<TTime, TValue> Empty
		{
			get
		{
			return new Slice<TTime, TValue>(s_emptyBuf, 0, 0, 0);
		}
		}

		public bool IsEmpty() { return Duration == 0; }

		/// <summary>
		/// For use by extension methods only
		/// </summary>
		internal Buf<TValue> Buffer{ get{ return m_buffer; } }

			/// <summary>
			/// For use by extension methods only
			/// </summary>
		internal Duration<TTime> Offset{ get{ return m_offset; } }

			public TValue this[Duration<TTime> offset, int subindex]
		{
			get
		{
			Duration<TTime> totalOffset = m_offset + offset;
		Debug.Assert(totalOffset * SliverSize < Buffer.Data.Length);
		long finalOffset = totalOffset * SliverSize + subindex;
		return m_buffer.Data[finalOffset];
		}
			set
		{
			Duration<TTime> totalOffset = m_offset + offset;
		Debug.Assert(totalOffset < (Buffer.Data.Length / SliverSize));
		long finalOffset = totalOffset * SliverSize + subindex;
		m_buffer.Data[finalOffset] = value;
		}
		}

			/// <summary>
			/// Get a portion of this Slice, starting at the given offset, for the given duration.
			/// </summary>
			public Slice<TTime, TValue> Subslice(Duration<TTime> initialOffset, Duration<TTime> duration)
		{
			Debug.Assert(initialOffset >= 0); // can't slice before the beginning of this slice
			Debug.Assert(duration >= 0); // must be nonnegative count
			Debug.Assert(initialOffset + duration <= Duration); // can't slice beyond the end
			return new Slice<TTime, TValue>(m_buffer, m_offset + initialOffset, duration, SliverSize);
		}

		/// <summary>
		/// Get the rest of this Slice starting at the given offset.
		/// </summary>
		public Slice<TTime, TValue> SubsliceStartingAt(Duration<TTime> initialOffset)
		{
			return Subslice(initialOffset, Duration - initialOffset);
		}

		/// <summary>
		/// Get the prefix of this Slice starting at offset 0 and extending for the requested duration.
		/// </summary>
		public Slice<TTime, TValue> SubsliceOfDuration(Duration<TTime> duration)
		{
			return Subslice(0, duration);
		}

		/// <summary>
		/// Copy this slice's data into destination; destination must be long enough.
		/// </summary>
		public void CopyTo(Slice<TTime, TValue> destination)
		{
			Debug.Assert(destination.Duration >= Duration);
			Debug.Assert(destination.SliverSize == SliverSize);

			// TODO: support backwards copies etc.
			Array.Copy(
				m_buffer.Data,
				(int)m_offset * SliverSize,
				destination.m_buffer.Data,
				(int)destination.m_offset * destination.SliverSize,
				(int)Duration * SliverSize);
		}

		public void CopyFrom(TValue[] source, int sourceOffset, int destinationSubIndex, int subWidth)
		{
			Debug.Assert((int)(sourceOffset + subWidth) <= source.Length);
			int destinationOffset = (int)(m_offset * SliverSize + destinationSubIndex);
			Debug.Assert(destinationOffset + subWidth <= m_buffer.Data.Length);

			Array.Copy(
				source,
				(int)sourceOffset,
				m_buffer.Data,
				destinationOffset,
				subWidth);
		}

		/// <summary>Are these samples adjacent in their underlying storage?</summary>
		public bool Precedes(Slice<TTime, TValue> next)
		{
			return m_buffer.Data == next.m_buffer.Data && m_offset + Duration == next.m_offset;
		}

		/// <summary>Merge two adjacent samples into a single sample.</summary>
		public Slice<TTime, TValue> UnionWith(Slice<TTime, TValue> next)
		{
			Contract.Assert(Precedes(next));
			return new Slice<TTime, TValue>(m_buffer, m_offset, Duration + next.Duration, SliverSize);
		}

		public override string ToString()
		{
			return "Slice[buffer " + m_buffer + ", offset " + m_offset + ", duration " + Duration + ", sliverSize " + SliverSize + "]";
		}

		/// <summary>
		/// Equality comparison; deliberately does not implement Equals(object) as this would cause slice boxing.
		/// </summary>
		public bool Equals(Slice<TTime, TValue> other)
		{
			return Buffer.Equals(other.Buffer) && Offset == other.Offset && Duration == other.Duration;
		}
	}
}

/// Copyright 2011-2017 by Rob Jellinghaus.  All rights reserved.

using System.Collections.Generic;

namespace Holofunk.Core
{
	/// <summary>
	/// A slice with an absolute initial time associated with it.
	/// </summary>
	/// <remarks>
	/// In the case of BufferedStreams, the first TimedSlice's InitialTime will be the InitialTime
	/// of the stream itself.
	/// </remarks>
	struct TimedSlice<TTime, TValue>
	where TValue : struct
	{
		internal readonly Time<TTime> InitialTime;
		internal readonly Slice<TTime, TValue> Slice;

		internal TimedSlice(Time<TTime> startTime, Slice<TTime, TValue> slice)
		{
			InitialTime = startTime;
			Slice = slice;
		}

		internal Interval<TTime> Interval{ get{ return new Interval<TTime>(InitialTime, Slice.Duration); } }

			internal class Comparer : IComparer<TimedSlice<TTime, TValue>>
		{
			internal static Comparer Instance = new Comparer();

			public int Compare(TimedSlice<TTime, TValue> x, TimedSlice<TTime, TValue> y)
			{
				if (x.InitialTime < y.InitialTime) {
					return -1;
				}
				else if (x.InitialTime > y.InitialTime) {
					return 1;
				}
				else {
					return 0;
				}
			}
		}

	}
}
