//--------------------------------------------------------------------------
//             EventQueue.cpp - limited msg queue functionality.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include "EventQueue.h"

EventQueue::EventQueue(std::unique_ptr<Pollable> obj) :
    pollObj(std::move(obj))
{
    setIsEmpty(true);
}

void EventQueue::Post(const HandlerFunc &handler)
{
    std::unique_lock<std::mutex> lock(mutex);

    bool wasEmpty = IsEmpty();
    queue.push(handler);
    if (wasEmpty)
    {
        setIsEmpty(false);
        if (pollObj != nullptr)
            pollObj->Ready();
    }
    lock.unlock();
    if (wasEmpty)
        cv.notify_one();
}

void EventQueue::Post(HandlerFunc &&handler)
{
    std::unique_lock<std::mutex> lock(mutex);

    bool wasEmpty = IsEmpty();
    queue.emplace(handler);
    if (wasEmpty)
    {
        setIsEmpty(false);
        if (pollObj != nullptr)
            pollObj->Ready();
    }
    lock.unlock();
    if (wasEmpty)
        cv.notify_one();
}

void EventQueue::RunOne()
{
    std::unique_lock<std::mutex> lock(mutex);
    if (IsEmpty())
        cv.wait(lock, [this] { return !IsEmpty(); });
    HandlerFunc func(std::move(queue.front()));
    queue.pop();
    if (queue.empty())
    {
        setIsEmpty(true);
        if (pollObj != nullptr)
            pollObj->NotReady();
    }
    lock.unlock();
    func();
}

int EventQueue::pollOneSlowPath()
{
    std::unique_lock<std::mutex> lock(mutex);
    if (IsEmpty())
        return 0;
    HandlerFunc func(std::move(queue.front()));
    queue.pop();
    if (queue.empty())
    {
        setIsEmpty(true);
        if (pollObj != nullptr)
            pollObj->NotReady();
    }
    lock.unlock();
    func();
    return 1;
}

const Pollable *EventQueue::PollableObject() const
{
    std::unique_lock<std::mutex> lock(mutex);
    return pollObj.get();
}

void EventQueue::SetPollableObject(std::unique_ptr<Pollable> obj)
{
    std::unique_lock<std::mutex> lock(mutex);
    pollObj = std::move(obj);
    if (!IsEmpty() && pollObj != nullptr)
        pollObj->Ready();
}
