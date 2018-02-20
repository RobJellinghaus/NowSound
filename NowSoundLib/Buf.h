
// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

namespace NowSound
{
    // Buffer of data; owns the data contained within it.
    template<typename T>
    class OwningBuf
    {
        int _id;
        std::unique_ptr<T> _data;
        int _length;

    public:
        OwningBuf() = delete;

        // Construct an OwningBuf on the given data, which must have at least one element.
        OwningBuf(int id, std::unique_ptr<T>&& data, int length)
            : _id(id), _data(std::move(data)), _length(length)
        {
            Check(_data != nullptr);
            Check(length > 0);
        }

        // move constructor
        OwningBuf(OwningBuf&& other)
            : _id(other._id), _data(std::move(other._data)), _length(other._length)
        {
            Check((_data == nullptr) == (_length == 0));

            Check(other._data == nullptr);
            other._length = 0;
        }

        int Id() const { return _id; }
        T* Data() const { return _data.get(); }
        int Length() const { return _length; }

        bool operator==(const OwningBuf<T>& other) const
        {
            return _id == other._id && _data == other._data && _length == other._length;
        }

        OwningBuf<T>& operator=(OwningBuf<T>&& other)
        {
            _id = other._id;
            _data = std::move(other._data);
            _length = other._length;

            Check((_data == nullptr) == (_length == 0));
            Check(other._data == nullptr);
            other._length = 0;

            return *this;
        }
    };

    // Slice allocator interface for freeing a slice; slices are asked to free themselves so they don't publicly
    // expose an rvalue reference operator.
    template<typename TValue>
    class IBufAllocator
    {
    public:
        virtual void Free(OwningBuf<TValue>&& buf) = 0;
    };

    // Non-owning, pass-by-value reference to the data owned by an OwningBuf<T>.
    // TODO: consider converting this to span<T>.
    template<typename T>
    class Buf
    {
        T* _data;
        int _length;

    public:
        // Default Buf is useful for zero-initializing an empty buffer; since this uses value semantics, this is reasonable
        Buf() : _data{}, _length{} { }

        Buf(OwningBuf<T>& owningBuf) : _data{owningBuf.Data()}, _length{owningBuf.Length()}
        {
            Check(_data != nullptr);
            Check(_length > 0);
        }

        T* Data() const { return _data; }
        int Length() const { return _length; }

        // allow default copy (and hence assignment)
        Buf(const Buf<T>& other) = default;
    };
}