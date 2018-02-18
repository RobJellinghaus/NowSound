
// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

namespace NowSound
{
    // Buffer of data; owns the data contained within it.
    template<typename T>
    class Buf
    {
        int _id;
        std::unique_ptr<T> _data;
        int _length;

    public:
        Buf(int id, std::unique_ptr<T>&& data, int length)
            : _id(id), _data(std::move(data)), _length(length)
        {
            Check(_data != nullptr);
        }

        // move constructor
        Buf(Buf&& other)
            : _id(other._id), _data(std::move(other._data)), _length(other._length)
        {
            Check(_data != nullptr);
        }

        int Id() const { return _id; }
        T* Data() const { return _data.get(); }
        int Length() const { return _length; }

        bool operator==(const Buf<T>& other) const
        {
            return _id == other._id && _data == other._data && _length == other._length;
        }
    };

    // Slice allocator interface for freeing a slice; slices are asked to free themselves so they don't publicly
    // expose an rvalue reference operator.
    template<typename TValue>
    class IBufAllocator
    {
    public:
        virtual void Free(Buf<TValue>&& buf) = 0;
    };
}