//--------------------------------------------------------------------------
//             EventQueue.h - limited msg queue functionality.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

// EventQueues provide limited event-loop functionality.  One can post (and on
//  the other end, run) "messages" in the form of handlers.
// Handlers are used instead of explicit message types to avoid the need for
//  marshalling code.  However this comes at the expense of reduced opacity
//  between sender and receiver.
// The (few) advantages of an EventQueue over something like (for example) a
//  boost::asio:io_service is:
// 1) EventQueue::PollOne() is lockless and fast in the common case of no
//    pending work, and
// 2) Thanks to the integration with Pollable, EventQueues are composable with
//    any other descriptors that need to be waited on.
// A tradeoff here is that complicated handlers (those built through
//  std::bind()) will probably do at least one heap allocation.
#ifndef EVENTQUEUE_H
#define EVENTQUEUE_H

#include <atomic>             // std::atomic_bool
#include <condition_variable> // std::condition_variable
#include <memory>             // std::unique_ptr
#include <queue>              // std::queue

#include "Pollable.h"
#include "SimpleQueue.h"

#if !defined(__alpha__) && !defined(__mips__)
// Assume volatile variable writes can be read by other threads.
#define HAS_COHERENT_CACHE
#endif

class EventQueue
{
public:
    // Consumes 'obj', when !nullptr.
    EventQueue(std::unique_ptr<Pollable> obj = nullptr);

    using HandlerFunc = std::function<void()>;
    
    void Post(const HandlerFunc &handler);
    void Post(HandlerFunc &&handler);
    int PollOne(); // Runs one event if the queue is non-empty.  Returns number
                   //  of events executed (0 or 1).
    void RunOne(); // Blocks until one event is ready, then runs it
    
    // Of course, this may change if another thread is altering the queue.
    inline bool IsEmpty() const;
    
    const Pollable *PollableObject() const;
    // Consumes 'obj', when !nullptr.  Nothing should be polling on the current
    //  object, as it will be closed/invalidated.  'obj' should also be in an
    //  'empty' state.
    void SetPollableObject(std::unique_ptr<Pollable> obj);
    
private:
    // Performance varies (slightly) depending on the member variable
    //  arrangement.  This is the best one I could find for my platform.
    // A SimpleQueue appears to behave ever very slightly (.02%) better on
    //  average than a std::deque for a standard search workload.
    std::queue<HandlerFunc, arctic::SimpleQueue<HandlerFunc>> queue;
    std::unique_ptr<Pollable> pollObj;
    std::condition_variable cv;
    mutable std::mutex mutex;
    // With a degenerate vector implementation and arbitrary concurrent
    //  operations, it might not be safe to directly query queue.empty() w/out
    //  locking.
#ifdef HAS_COHERENT_CACHE
    volatile bool isEmpty;
#else    
    std::atomic_bool isEmpty;
#endif
    int pollOneSlowPath();
    inline void setIsEmpty(bool val);
};

inline bool EventQueue::IsEmpty() const
{
#ifdef HAS_COHERENT_CACHE
    return isEmpty;
#else    
    return isEmpty.load(std::memory_order_relaxed);
#endif
}

inline void EventQueue::setIsEmpty(bool val)
{
#ifdef HAS_COHERENT_CACHE
    isEmpty = val;
#else    
    isEmpty.store(val, std::memory_order_relaxed);
#endif
}

inline int EventQueue::PollOne()
{
    return IsEmpty() ? 0 : pollOneSlowPath();
}

#endif // EVENTQUEUE_H
