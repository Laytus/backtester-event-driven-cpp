#include "backtester/engine/backtest_engine.hpp"

#include <utility>

namespace backtester::engine {

BacktestEngine::BacktestEngine(data::HistoricalDataFeed data_feed)
    : data_feed_(std::move(data_feed)) {}

std::optional<events::Event> BacktestEngine::step() {
    if (!event_queue_.empty()) {
        return event_queue_.pop();
    }

    if (data_feed_exhausted_) {
        return std::nullopt;
    }

    auto bar = data_feed_.next();

    if (!bar.has_value()) {
        data_feed_exhausted_ = true;
        return std::nullopt;
    }

    event_queue_.push(
        events::Event{
            events::MarketEvent{
                std::move(*bar)
            }
        }
    );

    return event_queue_.pop();
}

bool BacktestEngine::finished() const noexcept {
    return data_feed_exhausted_ && event_queue_.empty();
}

}  // namespace backtester::engine