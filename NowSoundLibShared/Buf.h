// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "stdafx.h"
#include "Check.h"

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

        // Create a new OwningBuf with a newly allocated T[length] backing store.
        OwningBuf(int id, int length)
            : _id(id), _data(std::unique_ptr<T>(new T[length])), _length(length)
        {
            Check(length > 0);
        }

        // Create an OwningBuf which takes ownership of rawBuffer (which had better have the given length).
        OwningBuf(int id, int length, T* rawBuffer)
            : _id(id), _data(std::unique_ptr<T>(rawBuffer)), _length(length)
        {
            Check(length > 0);
        }

        // Move constructor.
        OwningBuf(OwningBuf&& other)
            : _id(other._id), _data(std::move(other._data)), _length(other._length)
        {
            Check((_data == nullptr) == (_length == 0));

            Check(other._data == nullptr);
            other._length = 0;
        }

        // ID of this owning buffer; primarily for debugging.
        int Id() const { return _id; }
        // Borrowed pointer to the actual data.
        T* Data() const { return _data.get(); }
        // Count of T values in the actual data, NOT count of individual slices.
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
    // Knows nothing about slice size.
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

        // Borrowed pointer to the actual data.
        T* Data() const { return _data; }
        // Length of actual data; count of T values (NOT individual slices).
        int Length() const { return _length; }

        // allow default copy (and hence assignment)
        Buf(const Buf<T>& other) = default;
    };
}