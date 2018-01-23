// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

namespace NowSound
{
	/// <summary>Rolling buffer which can average a number of T's.</summary>
	/// <remarks>Parameterized with methods to handle summing / dividing the T's in question.</remarks>
	public abstract class Averager<T>
{
	// the buffer of T's
	readonly T[] m_storage;

	// have we filled the current storage?
	bool m_storageFull;

	// what's the next index to be overwritten with the next datum?
	int m_index;

	// the total
	T m_total;

	// the current average, so we don't have race conditions about it
	T m_average;

	public Averager(int capacity)
	{
		m_storage = new T[capacity];
	}

	/// <summary>Has this Averager got no data?</summary>
	public bool IsEmpty{ get{ return m_index == 0 && !m_storageFull; } }

		/// <summary>Update this Averager with another data point.</summary>
		public void Update(T nextT)
	{
		if (!IsValid(nextT)) {
			return;
		}

		if (m_index == m_storage.Length) {
			// might as well unconditionally set it, branching is more expensive
			m_storageFull = true;
			m_index = 0;
		}

		if (m_storageFull) {
			m_total = Subtract(m_total, m_storage[m_index]);
		}
		m_total = Add(m_total, nextT);
		m_storage[m_index] = nextT;
		m_index++;
		m_average = Divide(m_total, m_storageFull ? m_storage.Length : m_index);
	}

	/// <summary>Get the average; invalid if Average.IsEmpty.</summary>
	public T Average
	{
		get
	{
		return m_average;
	}
	}

	protected abstract bool IsValid(T t);
	protected abstract T Subtract(T total, T nextT);
	protected abstract T Add(T total, T nextT);
	protected abstract T Divide(T total, int count);
}

public class FloatAverager : Averager<float>
{
	public FloatAverager(int capacity)
		: base(capacity)
	{
	}

	protected override bool IsValid(float t)
	{
		// semi-arbitrary, but intended to filter out infinities and other extreme bogosities
		return -100 < t && t < 2000;
	}

	protected override float Add(float total, float nextT)
	{
		return total + nextT;
	}

	protected override float Subtract(float total, float nextT)
	{
		return total - nextT;
	}

	protected override float Divide(float total, int count)
	{
		return total / count;
	}
}

public class Vector3Averager : Averager<Vector3>
{
	public Vector3Averager(int capacity)
		: base(capacity)
	{
	}

	protected override bool IsValid(Vector3 t)
	{
		// semi-arbitrary, but intended to filter out infinities and other extreme bogosities
		return -100 < t.x && t.x < 2000 && -100 < t.y && t.y < 2000;
	}

	protected override Vector3 Add(Vector3 total, Vector3 nextT)
	{
		return total + nextT;
	}

	protected override Vector3 Subtract(Vector3 total, Vector3 nextT)
	{
		return total - nextT;
	}

	protected override Vector3 Divide(Vector3 total, int count)
	{
		return new Vector3(total.x / count, total.y / count, total.z / count);
	}
}
}
