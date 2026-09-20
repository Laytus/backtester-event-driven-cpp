#pragma once

#include "backtester/data/historical_data_feed.hpp"
#include "backtester/events/event.hpp"
#include "backtester/events/event_queue.hpp"

#include <optional>

namespace backtester::engine {

class BacktestEngine {
public:
    explicit BacktestEngine(data::HistoricalDataFeed data_feed);

    BacktestEngine(const BacktestEngine&) = delete;
    BacktestEngine& operator=(const BacktestEngine&) = delete;

    BacktestEngine(BacktestEngine&&) = default;
    BacktestEngine& operator=(BacktestEngine&&) = default;

    [[nodiscard]]
    std::optional<events::Event> step();

    [[nodiscard]]
    bool finished() const noexcept;

private:
    data::HistoricalDataFeed data_feed_;
    events::EventQueue event_queue_;
    bool data_feed_exhausted_{false};
};

}  // namespace backtester::engine