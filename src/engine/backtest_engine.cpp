#include "backtester/engine/backtest_engine.hpp"

#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace {

template <typename>
inline constexpr bool dependent_false_v = false;

}  // namespace

namespace backtester::engine {

BacktestEngine::BacktestEngine(
    data::HistoricalDataFeed data_feed,
    std::unique_ptr<strategy::Strategy> strategy,
    portfolio::Portfolio portfolio,
    portfolio::PositionSizer position_sizer
)
    : data_feed_(std::move(data_feed)),
    strategy_(std::move(strategy)),
    portfolio_(std::move(portfolio)),
    position_sizer_(std::move(position_sizer)) {
        if (strategy_ == nullptr) {
            throw std::invalid_argument(
                "BacktestEngine requires a strategy"
            );
        }
    }

std::optional<events::Event> BacktestEngine::step() {
    if (finished()) {
        return std::nullopt;
    }

    if (event_queue_.empty()) {
        if (active_bar_.has_value()) {
            throw std::logic_error(
                "Active session has no pending events"
            );
        }

        if (!load_next_session()) {
            return std::nullopt;
        }
    }

    auto event = event_queue_.pop();

    if (!event.has_value()) {
        throw std::logic_error(
            "BacktestEngine expected a pending event"
        );
    }

    process_event(*event);

    complete_active_session_if_ready();

    return event;
}

bool BacktestEngine::finished() const noexcept {
    return data_feed_exhausted_
        && event_queue_.empty()
        && !active_bar_.has_value()
        && !execution_handler_.has_pending_order();
}

const portfolio::Portfolio& BacktestEngine::portfolio() const noexcept {
    return portfolio_;
}

bool BacktestEngine::load_next_session() {
    if (active_bar_.has_value()) {
        throw std::logic_error(
            "Cannot load a new session while another session is active"
        );
    }

    if (!event_queue_.empty()) {
        throw std::logic_error(
            "Cannot load a new session while events are pending"
        );
    }

    if (data_feed_exhausted_) {
        return false;
    }

    auto next_bar = data_feed_.next();

    if (!next_bar.has_value()) {
        data_feed_exhausted_ = true;
        execution_handler_.discard_pending();

        return false;
    }

    active_bar_ = std::move(*next_bar);

    if (active_bar_->symbol() != portfolio_.symbol()) {
        throw std::invalid_argument(
            "Market data symbol does not match Portfolio symbol"
        );
    }

    auto fill = execution_handler_.execute_on_open(
        active_bar_->session_date(),
        active_bar_->open(),
        portfolio_
    );

    if (fill.has_value()) {
        event_queue_.push(
            events::Event{std::move(*fill)}
        );
    }

    event_queue_.push(
        events::Event{
            events::MarketEvent{*active_bar_}
        }
    );

    return true;
}

void BacktestEngine::process_event(const events::Event& event) {
    std::visit(
        [this](const auto& concrete_event) {
            using EventType = std::decay_t<decltype(concrete_event)>;

            if constexpr (
                std::is_same_v<
                    EventType,
                    events::MarketEvent
                >
            ) {
                process_market_event(concrete_event);
            } else if constexpr (
                std::is_same_v<
                    EventType,
                    events::SignalEvent
                >
            ) {
                process_signal_event(concrete_event);
            } else if constexpr (
                std::is_same_v<
                    EventType,
                    events::OrderEvent
                >
            ) {
                process_order_event(concrete_event);
            } else if constexpr (
                std::is_same_v<
                    EventType,
                    events::FillEvent
                >
            ) {
                process_fill_event(concrete_event);
            } else {
                static_assert(
                    dependent_false_v<EventType>,
                    "BacktestEngine does not handle this Event type"
                );
            }
        },
        event
    );
}

void BacktestEngine::process_market_event(
    const events::MarketEvent& event
) {
    auto signals = strategy_->on_market(event);

    for (auto& signal : signals) {
        event_queue_.push(
            events::Event{std::move(signal)}
        );
    }
}

void BacktestEngine::process_signal_event(
    const events::SignalEvent& event
) {
    if (!active_bar_.has_value()) {
        throw std::logic_error(
            "SignalEvent processed without an active session"
        );
    }

    if (event.signal_date() != active_bar_->session_date()) {
        throw std::logic_error(
            "SignalEvent date does not match active session"
        );
    }

    const auto target_position =
        position_sizer_.target_position(
            event,
            portfolio_,
            active_bar_->close()
        );

    auto order = order_generator_.generate(
        event,
        portfolio_,
        target_position
    );

    if (order.has_value()) {
        event_queue_.push(
            events::Event{std::move(*order)}
        );
    }
}

void BacktestEngine::process_order_event(
    const events::OrderEvent& event
) {
    execution_handler_.submit(event);
}

void BacktestEngine::process_fill_event(
    const events::FillEvent& event
) {
    portfolio_.apply_fill(event);
}

void BacktestEngine::complete_active_session_if_ready() {
    if (!active_bar_.has_value()) {
        return;
    }

    if (!event_queue_.empty()) {
        return;
    }

    portfolio_.mark_to_market(
        active_bar_->session_date(),
        active_bar_->close()
    );

    active_bar_.reset();
}

}  // namespace backtester::engine