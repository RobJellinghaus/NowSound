// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include "stdint.h"

#include <deque>

// Simple histogram class for tracking a bounded history of data values.
class HistogramBase
{
protected:
    int _capacity;

    float _max;
    float _min;
    float _average;

    HistogramBase(int capacity) : _capacity{ capacity }, _max{}, _min{}, _average{}
    {}

public:
    float Max() const { return _max; }
    float Min() const { return _min; }
    float Average() const { return _average; }

    // Add a new value to this histogram.
    // Return true if the histogram is not yet at capacity; false if it is at capacity.
    // (In the case of More can still be added if it is at capacity, but earlier values will be dropped
    // and max/min values must be recomputed.)
    virtual bool Add(float value) = 0;
};

// Basic implementation of HistogramBase, keeping a deque of floats.
// TODO: possible other implementations (histogram of histograms) if scalability becomes an issue.
class Histogram : public HistogramBase
{
private:
    std::deque<float> _values;

public:
    Histogram(int capacity) : HistogramBase(capacity), _values{}
    {}

    // TODO: this shouldn't be in header
    virtual bool Add(float value)
    {
        bool atCapacity = _values.size() == _capacity;

        if (atCapacity)
        {
            _values.pop_front();
        }

        _values.push_back(value);

        // have to recompute max/min.
        // TODO: this is expensive; if bottlenecked, implement compound histogram.
        _max = _values[0];
        _min = _values[0];
        float total = _values[0];
        for (int i = 1; i < _values.size(); i++)
        {
            _max = _max > _values[i] ? _max : _values[i];
            _min = _min > _values[i] ? _values[i] : _min;
            total += _values[i];
        }
        _average = total / _values.size();

        return !atCapacity;
    }
};
