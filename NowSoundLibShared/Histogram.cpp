// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

#include "Histogram.h"

using namespace NowSound;

bool Histogram::Add(float value)
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
