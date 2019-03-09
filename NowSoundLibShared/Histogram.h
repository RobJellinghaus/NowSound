// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdint.h"

#include <deque>

// Simple histogram structure for tracking statistics over a bounded set of float values.
// Average() is always available with O(1) performance.  Max() and Min() are calculated lazily on demand.
class Histogram
{
private:
	int _capacity;

	// The possibly stale values of _min and _max (if !_minMaxKnown).
	float _min;
	float _max;

	// The always accurate total of all values.
	float _total;

	// The always accurate average of all values.
	// This is precomputed in order to be readable thread-safely (assuming float writes are atomic...).
	float _average;

	// If _max and _min are known to be accurate with respect to _valuesInInsertionOrder, then this is true.
	bool _minMaxKnown;
	
	// This deque simply tracks the values in the order they entered the histogram.
	std::deque<float> _valuesInInsertionOrder;

	// Mutex to prevent collision between Add() and EnsureMinMaxKnown().
	std::mutex _mutex;

	// Recalculate min/max if necessary.
	void EnsureMinMaxKnown();

	// Add implementation (no locking).
	void AddImpl(float value);

public:
	Histogram(int capacity);

	float Min();
	float Max();
	float Average() const;

	// Add a new value to this histogram.
	void Add(float value);

	void AddAll(const float* data, int count, bool absoluteValue);
};
