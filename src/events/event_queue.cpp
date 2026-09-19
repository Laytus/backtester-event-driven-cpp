#include "backtester/events/event_queue.hpp"

#include <utility>

namespace backtester::events {

void EventQueue::push(Event event) {
    events_.push(std::move(event));
}

std::optional<Event> EventQueue::pop() {
    if (events_.empty()) {
        return std::nullopt;
    }

    Event event = std::move(events_.front());
    events_.pop();

    return event;
}

bool EventQueue::empty() const noexcept {
    return events_.empty();
}

}  // namespace backtester::events