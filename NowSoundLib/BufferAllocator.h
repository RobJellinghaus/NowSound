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

    // 
    // Allocate T[] of a predetermined size, and support returning such T[] to a free list.
    // 
    template<typename T>
    class BufferAllocator
    {
    private:
        int _latestBufferId = 1; // 0 = empty buf

    public:
        // The number of T in a buffer from this allocator.
        const int BufferLength;

    private:
        // Free list; we recycle from here if possible.
        // This allocator owns all these buffers.
        const std::vector<Buf<T>> _freeList;

        // Total number of buffers we have ever allocated.
        int _totalBufferCount;

    public:
        // bufferCount is the number of values in each buffer; initialNumberOfBuffers is the number of buffers to pre-allocate
        BufferAllocator(int bufferLength, int initialNumberOfBuffers)
        {
            Check(bufferLength > 0);
            Check(initialNumberOfBuffers > 0);

            BufferLength = bufferLength;

            // Prepopulate the free list as a way of preallocating.
            for (int i = 0; i < initialNumberOfBuffers; i++)
            {
                _freeList.emplace(std::move(Buf<T>(_latestBufferId++, new T[BufferLength], bufferLength)));
            }
            _totalBufferCount = initialNumberOfBuffers;
        }

        // no copying this
        BufferAllocator(const BufferAllocator&) = delete;

        // Number of bytes reserved by this allocator; will increase if free list runs out, and includes free space.
        long TotalReservedSpace() { return _totalBufferCount * BufferLength * sizeof(T); }

        // Number of bytes held in buffers on the free list.
        long TotalFreeListSpace() { return _freeList.Count * BufferLength * sizeof(T); }

        // Allocate a new Buf<T>, returned by rvalue reference.
        // TODO: needs thread safety or contractual thread affinity
        Buf<T>&& Allocate()
        {
            if (_freeList.Count == 0)
            {
                _totalBufferCount++;
                return std::move(Buf<T>(_latestBufferId++, new T[BufferLength], BufferLength));
            }
            else
            {
                Buf<T> ret = std::move(_freeList[_freeList.Count - 1]);
                _freeList.RemoveAt(_freeList.Count - 1);
                return std::move(ret);
            }
        }

        // Free the given buffer back to the pool.
        void Free(Buf<T>&& buffer)
        {
            for (const Buf<T>& t : _freeList)
            {
                if (t.Data == buffer.Data)
                {
                    return;
                }
            }
            _freeList.append(std::move(buffer));
        }
    };
}
