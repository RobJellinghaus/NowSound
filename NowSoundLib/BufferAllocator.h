// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

namespace NowSound
{
	public struct Buf<T>
{
	public readonly int Id;
	public readonly T[] Data;

	public Buf(int id, T[] data)
	{
		Id = id;
		Data = data;
	}

	public override string ToString()
	{
		return "Buf#" + Id + "[" + Data.Length + "]";
	}

	public bool Equals(Buf<T> other)
	{
		return Id == other.Id && Data == other.Data;
	}
}

// 
// Allocate T[] of a predetermined size, and support returning such T[] to a free list.
// 
public class BufferAllocator<T>
	where T : struct
	{
		int m_latestBufferId = 1; // 0 = empty buf

								  // 
								  // The number of T in a buffer from this allocator.
								  // 
		public readonly int BufferSize;

		// 
		// Free list; we recycle from here if possible.
		// 
		readonly List<Buf<T>> m_freeList = new List<Buf<T>>();

		readonly int m_sizeOfT;

		// 
		// Total number of buffers we have ever allocated.
		// 
		int m_totalBufferCount;

		public BufferAllocator(int bufferSize, int initialBufferCount, int sizeOfT)
		{
			BufferSize = bufferSize;
			m_sizeOfT = sizeOfT;

			for (int i = 0; i < initialBufferCount; i++) {
				m_freeList.Add(new Buf<T>(m_latestBufferId++, new T[BufferSize]));
			}
			m_totalBufferCount = initialBufferCount;
		}

		// 
		// Number of bytes reserved by this allocator; will increase if free list runs out, and includes free space.
		// 
		public long TotalReservedSpace{ get{ return m_totalBufferCount * BufferSize; } }

			// 
			// Number of bytes held in buffers on the free list.
			// 
		public long TotalFreeListSpace{ get{ return m_freeList.Count * BufferSize; } }

			public Buf<T> Allocate()
		{
			if (m_freeList.Count == 0) {
				m_totalBufferCount++;
				return new Buf<T>(m_latestBufferId++, new T[BufferSize]);
			}
			else {
				Buf<T> ret = m_freeList[m_freeList.Count - 1];
				m_freeList.RemoveAt(m_freeList.Count - 1);
				return ret;
			}
		}

		public void Free(Buf<T> buffer)
		{
			foreach(Buf<T> t in m_freeList) {
				if (t.Data == buffer.Data) {
					return;
				}
			}
			m_freeList.Add(buffer);
		}
	}
}
