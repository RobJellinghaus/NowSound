// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "pch.h"

namespace NowSound
{
    // Buffer of data; owns the data contained within it.
    template<typename T>
    struct Buf
    {
        const int Id;
        const T[] Data;

        Buf(int id, T[]&& data)
        {
            Id = id;
            Data = std::move(data);
        }

        // move constructor
        Buf(Buf&& other)
        {
            Id = std::move(other.Id);
            Data = std::move(other.Data);
        }

        bool Equals(const Buf<T>& other)
        {
            return Id == other.Id && Data == other.Data;
        }
    }

    // 
    // Allocate T[] of a predetermined size, and support returning such T[] to a free list.
    // 
    template<typename T>
    class BufferAllocator<T>
    {
    private:
        int _latestBufferId = 1; // 0 = empty buf

    public:
        // The number of T in a buffer from this allocator.
        const int BufferSize;

    private:
        // Free list; we recycle from here if possible.
        const std::vector<Buf<T>> _freeList;

        // Total number of buffers we have ever allocated.
        int _totalBufferCount;

    public:
        BufferAllocator(int bufferSize, int initialBufferCount)
        {
            Check(bufferSize > 0);
            Check(initialBufferCount > 0);

            BufferSize = bufferSize;

            for (int i = 0; i < initialBufferCount; i++)
            {
                _freeList.Add(new Buf<T>(_latestBufferId++, new T[BufferSize]));
            }
            _totalBufferCount = initialBufferCount;
        }

        // Number of bytes reserved by this allocator; will increase if free list runs out, and includes free space.
        long TotalReservedSpace() { return _totalBufferCount * BufferSize * sizeof(T); }

        // Number of bytes held in buffers on the free list.
        long TotalFreeListSpace() { return _freeList.Count * BufferSize * sizeof(T); }

        // Allocate a new Buf<T>, returned by rvalue reference.
        Buf<T>&& Allocate()
        {
            if (_freeList.Count == 0)
            {
                _totalBufferCount++;
                return std::move(new Buf<T>(_latestBufferId++, new T[BufferSize]));
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
                if (t.Data == buffer.Data) {
                    return;
                }
            }
            _freeList.append(std::move(buffer));
        }
    }
}
