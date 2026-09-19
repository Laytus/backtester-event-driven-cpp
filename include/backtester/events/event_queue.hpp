#pragma once

#include "backtester/events/event.hpp"

#include <optional>
#include <queue>

namespace backtester::events {

class EventQueue {
public:
    EventQueue() = default;

    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;

    EventQueue(EventQueue&&) = default;
    EventQueue& operator=(EventQueue&&) = default;

    void push(Event event);

    [[nodiscard]]
    std::optional<Event> pop();

    [[nodiscard]]
    bool empty() const noexcept;

private:
    std::queue<Event> events_;
};

}  // namespace backtester::events