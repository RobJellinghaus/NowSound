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
        std::vector<OwningBuf<T>> _freeList;

        // Total number of buffers we have ever allocated.
        int _totalBufferCount;

    public:
        // bufferLength is the number of values in each buffer; initialNumberOfBuffers is the number of buffers to pre-allocate
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
                OwningBuf<T> buf(_latestBufferId++, std::move(bufferPtr), bufferLength);
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

        // Allocate a new Buf<T>; this is an owning Buf<T>.
        // TODO: needs thread safety or contractual thread affinity
        OwningBuf<T> Allocate()
        {
            if (_freeList.size() == 0)
            {
                _totalBufferCount++;
                T* bufArray = new T[BufferLength];
                std::unique_ptr<T> bufStorage(std::move(bufArray));
                OwningBuf<T> result(_latestBufferId++, std::move(bufStorage), BufferLength);
                return std::move(result);
            }
            else
            {
                OwningBuf<T> ret(std::move(_freeList[_freeList.size() - 1]));
                _freeList.erase(_freeList.end() - 1);
                return std::move(ret);
            }
        }

        // Free the given buffer back to the pool.
        virtual void Free(OwningBuf<T>&& buffer)
        {
            // must not already be on free list or we have a bug
            for (const OwningBuf<T>& t : _freeList)
            {
                Check(!(buffer == t)); // TODO: Buf<T>::operator!=
            }
            _freeList.push_back(std::move(buffer));
        }
    };
}
