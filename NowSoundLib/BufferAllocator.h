// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#pragma once

#include "pch.h"

#include "Buf.h"

namespace NowSound
{
    // Allocate T[] of a predetermined size, and support returning such T[] to a free list.
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
        std::vector<Buf<T>> _freeList;

        // Total number of buffers we have ever allocated.
        int _totalBufferCount;

    public:
        // bufferCount is the number of values in each buffer; initialNumberOfBuffers is the number of buffers to pre-allocate
        BufferAllocator(int bufferLength, int initialNumberOfBuffers)
            : BufferLength(bufferLength)
        {
            Check(bufferLength > 0);
            Check(initialNumberOfBuffers > 0);

            // Prepopulate the free list as a way of preallocating.
            for (int i = 0; i < initialNumberOfBuffers; i++)
            {
                T* bufferArray = new T[bufferLength];
                std::unique_ptr<T> bufferPtr(bufferArray);
                Buf<T> buf(_latestBufferId++, std::move(bufferPtr), bufferLength);
                _freeList.push_back(std::move(buf));
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
        virtual void Free(Buf<T>&& buffer)
        {
            // must not already be on free list or we have a bug
            for (const Buf<T>& t : _freeList)
            {
                Check(!(t == buffer)); // TODO: Buf<T>::operator!=
            }
            _freeList.push_back(std::move(buffer));
        }
    };
}
