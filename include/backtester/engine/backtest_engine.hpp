#pragma once

#include "backtester/data/historical_data_feed.hpp"
#include "backtester/events/event.hpp"
#include "backtester/events/event_queue.hpp"
#include "backtester/execution/execution_handler.hpp"
#include "backtester/market/bar.hpp"
#include "backtester/portfolio/portfolio.hpp"
#include "backtester/portfolio/position_sizer.hpp"
#include "backtester/portfolio/order_generator.hpp"
#include "backtester/strategy/strategy.hpp"

#include <memory>
#include <optional>

namespace backtester::engine {

class BacktestEngine {
public:
    BacktestEngine(
        data::HistoricalDataFeed data_feed,
        std::unique_ptr<strategy::Strategy> strategy,
        portfolio::Portfolio portfolio,
        portfolio::PositionSizer position_sizer
    );

    BacktestEngine(const BacktestEngine&) = delete;
    BacktestEngine& operator=(const BacktestEngine&) = delete;

    BacktestEngine(BacktestEngine&&) = default;
    BacktestEngine& operator=(BacktestEngine&&) = default;

    [[nodiscard]]
    std::optional<events::Event> step();

    [[nodiscard]]
    bool finished() const noexcept;

    [[nodiscard]]
    const portfolio::Portfolio& portfolio() const noexcept;

private:
    [[nodiscard]]
    bool load_next_session();

    void process_event(const events::Event& event);

    void process_market_event(
        const events::MarketEvent& event
    );

    void process_signal_event(
        const events::SignalEvent& event
    );

    void process_order_event(
        const events::OrderEvent& event
    );

    void process_fill_event(
        const events::FillEvent& event
    );

    void complete_active_session_if_ready();

    data::HistoricalDataFeed data_feed_;
    events::EventQueue event_queue_;

    std::unique_ptr<strategy::Strategy> strategy_;

    portfolio::Portfolio portfolio_;
    portfolio::PositionSizer position_sizer_;
    portfolio::OrderGenerator order_generator_;

    execution::ExecutionHandler execution_handler_;

    std::optional<market::Bar> active_bar_;

    bool data_feed_exhausted_{false};
};

}  // namespace backtester::engine