
// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

namespace NowSound
{
    // Buffer of data; owns the data contained within it.
    template<typename T>
    struct Buf
    {
        const int Id;
        const T* Data;
        const int Length;

        Buf(int id, T*&& data, int length)
        {
            Id = id;
            Data = std::move(data);
            Length = length;
        }

        // move constructor
        Buf(Buf&& other)
        {
            Id = std::move(other.Id);
            Data = std::move(other.Data);
            Length = other.Length;
        }

        bool Equals(const Buf<T>& other)
        {
            return Id == other.Id && Data == other.Data && Length == other.Length;
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