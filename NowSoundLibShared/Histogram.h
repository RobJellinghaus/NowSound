// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

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
    // (More can still be added if it is at capacity, but earlier values will be dropped
    // and max/min values must be recomputed at O(n) cost.)
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
    virtual bool Add(float value);
};
