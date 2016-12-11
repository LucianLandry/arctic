//--------------------------------------------------------------------------
//                SimpleQueue.h - simple queue functionality.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef SIMPLEQUEUE_H
#define SIMPLEQUEUE_H

#include <memory>  // std::allocator
#include <utility> // std::move

// A SimpleQueue internally stores its elements in one contiguous array.
// Only a very few limited operations are supported (enough to wrap a std::queue
//  around it).
// This is expected to be slightly faster than a std::deque when the queue size
//  doesn't expand much, and slower otherwise.

namespace arctic
{

template <typename Elem, typename Allocator = std::allocator<Elem>>
class SimpleQueue
{
public:
    using value_type = Elem;
    using reference = value_type &;
    using const_reference = const value_type &;
    using size_type = std::size_t;
    
    SimpleQueue();
    ~SimpleQueue();

    // It is undefined behavior to invoke front() or pop_front() on an empty
    //  SimpleQueue.
    inline reference front();
    inline reference back();
    inline bool empty() const;
    inline size_type capacity() const;
    size_type size() const;
    
    void reserve(int new_cap);
    inline void push_back(const Elem &elem);
    template <class... Args>
    inline void emplace_back(Args&&... args);
    inline void pop_front();
    inline void clear();
private:
    Allocator a;
    Elem *head;
    Elem *tail;
    Elem *elemStorage;
    Elem *elemStorageEnd; // points to 1 past the end of the array
    // When 'head' == 'tail', it is ambiguous whether the queue is empty or
    //  full (or both, when capacity == 0!)  So we use these flags to
    //  distinguish.
    bool isEmpty;
    bool isFull;
};

template <typename Elem, typename Allocator>
SimpleQueue<Elem, Allocator>::SimpleQueue() :
    head(nullptr), tail(nullptr), elemStorage(nullptr), elemStorageEnd(nullptr),
    isEmpty(true), isFull(true) {}

template <typename Elem, typename Allocator>
SimpleQueue<Elem, Allocator>::~SimpleQueue()
{
    clear();
    a.deallocate(elemStorage, capacity());
}

template <typename Elem, typename Allocator>
inline bool SimpleQueue<Elem, Allocator>::empty() const
{
    return isEmpty;
}

template <typename Elem, typename Allocator>
inline Elem &SimpleQueue<Elem, Allocator>::front()
{
    return *head;
}

template <typename Elem, typename Allocator>
inline Elem &SimpleQueue<Elem, Allocator>::back()
{
    return *tail;
}

template <typename Elem, typename Allocator>
inline typename SimpleQueue<Elem, Allocator>::size_type
SimpleQueue<Elem, Allocator>::capacity() const
{
    return elemStorageEnd - elemStorage;
}

template <typename Elem, typename Allocator>
typename SimpleQueue<Elem, Allocator>::size_type
SimpleQueue<Elem, Allocator>::size() const
{
    return
        head == tail ? (isEmpty ? 0 : capacity()) :
        head < tail ? tail - head :
        // Assume head > tail (queue currently wrapped around end)
        (tail - elemStorage) + (elemStorageEnd - head);
}

template <typename Elem, typename Allocator>
inline void SimpleQueue<Elem, Allocator>::pop_front()
{
    isFull = false;
    a.destroy(head);
    if (++head == elemStorageEnd)
        head = elemStorage;
    if (head == tail)
        isEmpty = true;
}

template <typename Elem, typename Allocator>
inline void SimpleQueue<Elem, Allocator>::clear()
{
    while (!isEmpty)
        pop_front();
}

template <typename Elem, typename Allocator>
inline void SimpleQueue<Elem, Allocator>::push_back(const Elem &elem)
{
    if (isFull)
        reserve(isEmpty ? 1 : capacity() * 2);
    // At this point we should have enough space to insert an element.
    isEmpty = false;
    a.construct(tail, elem); // invokes copy constructor
    if (++tail == elemStorageEnd)
        tail = elemStorage;
    if (tail == head)
        isFull = true;
}

template <typename Elem, typename Allocator>
template <class... Args>
inline void SimpleQueue<Elem, Allocator>::emplace_back(Args&&... args)
{
    if (isFull)
        reserve(isEmpty ? 1 : capacity() * 2);
    // At this point we should have enough space to insert an element.
    isEmpty = false;
    // invokes move-constructor (hopefully)
    a.construct(tail, std::forward<Args>(args)...);
    if (++tail == elemStorageEnd)
        tail = elemStorage;
    if (tail == head)
        isFull = true;
}

template <typename Elem, typename Allocator>
void SimpleQueue<Elem, Allocator>::reserve(int new_cap)
{
    int oldCapacity = capacity();
    if (new_cap <= oldCapacity)
        return; // refuse to let reserve() shrink capacity
    int oldSize = size();
    Elem *newMem = a.allocate(new_cap); // may throw std::bad_alloc
    Elem *newMemHead = newMem;
    
    // Move elements to beginning of the new memory.
    if (!isEmpty)
    {
        do
        {
            // assumes constructor does not throw
            a.construct(newMemHead++, std::move(*head));
            if (++head == elemStorageEnd)
                head = elemStorage;
        } while (head != tail);
    }

    a.deallocate(elemStorage, oldCapacity);
    elemStorage = newMem;
    elemStorageEnd = elemStorage + new_cap;
    head = elemStorage;
    tail = head + oldSize;
    isFull = false;
}
        
} // end namespace 'arctic'
    
#endif // SIMPLEQUEUE_H
