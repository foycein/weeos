#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>

namespace Ios
{

// Where a service puts the things that happen (a button press, a disc cover that opens) until the program asks for them.
// The original answers a long waiting request with an event. Here the producer posts it and the consumer waits for it.
// Events are kept in order. Any thread may post and any thread may wait.
template <typename Event>
class EventQueue
{
public:
    // Adds an event at the end. Does nothing when the queue is closed.
    void Post(Event event)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (isClosed)
                return;

            events.push_back(std::move(event));
        }

        somethingChanged.notify_one();
    }

    // Waits until an event is there. Gives back nothing when the queue is closed and empty, or when stopToken asks to stop.
    std::optional<Event> Wait(std::stop_token stopToken)
    {
        std::unique_lock<std::mutex> lock(mutex);
        somethingChanged.wait(lock, stopToken, [this] { return HasEventOrIsClosed(); });
        return TakeFirstEvent();
    }

    // Waits at most for the timeout. Gives back nothing when no event came in that time, or when the queue is closed and empty.
    std::optional<Event> WaitFor(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex);
        somethingChanged.wait_for(lock, timeout, [this] { return HasEventOrIsClosed(); });
        return TakeFirstEvent();
    }

    // Does not wait. Gives back nothing when there is no event.
    std::optional<Event> TryTake()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return TakeFirstEvent();
    }

    // Ends the queue: waiting threads wake up and no event can be posted any more. Events that are still in the queue can be taken.
    void Close()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            isClosed = true;
        }

        somethingChanged.notify_all();
    }

    bool IsClosed() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return isClosed;
    }

private:
    bool HasEventOrIsClosed() const
    {
        return !events.empty() || isClosed;
    }

    // The caller holds the lock.
    std::optional<Event> TakeFirstEvent()
    {
        if (events.empty())
            return std::nullopt;

        Event event = std::move(events.front());
        events.pop_front();
        return event;
    }

    mutable std::mutex mutex;
    std::condition_variable_any somethingChanged;
    std::deque<Event> events;
    bool isClosed = false;
};

}
