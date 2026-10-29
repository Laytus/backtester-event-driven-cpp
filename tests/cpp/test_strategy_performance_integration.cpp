
#include "backtester/data/historical_data_feed.hpp"
#include "backtester/engine/backtest_engine.hpp"
#include "backtester/events/event.hpp"
#include "backtester/indicators/relative_strength_index.hpp"
#include "backtester/market/bar.hpp"

#include "backtester/performance/active_return_series.hpp"
#include "backtester/performance/performance_metrics.hpp"
#include "backtester/performance/relative_metrics.hpp"
#include "backtester/performance/return_series.hpp"

#include "backtester/portfolio/portfolio.hpp"
#include "backtester/portfolio/position_sizer.hpp"

#include "backtester/strategy/strategy.hpp"
#include "backtester/strategy/price_source.hpp"
#include "backtester/strategy/buy_and_hold_strategy.hpp"
#include "backtester/strategy/sma_crossover_strategy.hpp"
#include "backtester/strategy/ema_crossover_strategy.hpp"
#include "backtester/strategy/price_filter_strategy.hpp"
#include "backtester/strategy/channel_breakout_strategy.hpp"
#include "backtester/strategy/momentum_strategy.hpp"
#include "backtester/strategy/macd_crossover_strategy.hpp"
#include "backtester/strategy/rsi_based_strategy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace {

using backtester::data::HistoricalDataFeed;
using backtester::engine::BacktestEngine;

using backtester::events::Event;
using backtester::events::FillEvent;
using backtester::events::MarketEvent;
using backtester::events::OrderEvent;
using backtester::events::SignalDirection;
using backtester::events::SignalEvent;

using backtester::indicators::RSIMode;

using backtester::market::Bar;
using backtester::market::SessionDate;

using backtester::performance::build_active_return_series;
using backtester::performance::build_return_series;
using backtester::performance::calculate_performance_metrics;
using backtester::performance::calculate_relative_performance_metrics;

using backtester::portfolio::Portfolio;
using backtester::portfolio::PortfolioSnapshot;
using backtester::portfolio::PositionQuantity;
using backtester::portfolio::PositionSizer;

using backtester::strategy::Strategy;
using backtester::strategy::PriceSource;
using backtester::strategy::BuyAndHoldStrategy;
using backtester::strategy::RSIBasedStrategy;
using backtester::strategy::RSISignalMode;

constexpr double initial_capital = 1'000.0;
constexpr double annualization_factor = 252.0;

// =============================================================================
// Shared fixtures
// =============================================================================

constexpr std::array<double, 11> oracle_prices{
    100.0, 99.0, 98.0, 102.0, 105.0, 104.0,
    100.0, 98.0, 103.0, 105.0, 101.0
};

SessionDate session_date(std::size_t index) {
    const std::chrono::sys_days first{
        std::chrono::year{2026} / std::chrono::October / 1
    };

    return SessionDate{
        first + std::chrono::days{static_cast<int>(index)}
    };
}

std::vector<Bar> oracle_bars(
    bool gap_at_first_buy = false
) {
    std::vector<Bar> bars;
    bars.reserve(oracle_prices.size());

    for (std::size_t i = 0; i < oracle_prices.size(); ++i) {
        const double price = oracle_prices[i];

        const double open =
            gap_at_first_buy && i == 3 ? 120.0 : price;

        const double high = std::max(open, price);
        const double low = std::min(open, price);

        bars.emplace_back(
            session_date(i),
            "AAPL",
            open,
            high,
            low,
            price,
            static_cast<std::uint64_t>(1'000 + i)
        );
    }

    return bars;
}

BacktestEngine make_rsi_engine(
    const std::vector<Bar>& bars
) {
    return BacktestEngine{
        HistoricalDataFeed{bars},
        std::make_unique<RSIBasedStrategy>(
            "AAPL",
            2,
            RSIMode::Directional,
            30.0,
            70.0,
            PriceSource::Close,
            RSISignalMode::DirectThreshold,
            0.5
        ),
        Portfolio{"AAPL", initial_capital},
        PositionSizer{1.0}
    };
}

// =============================================================================
// Shared event tracing
// =============================================================================

enum class EventKind {
    Market,
    Signal,
    Order,
    Fill
};

struct Trace final {
    std::vector<EventKind> kinds;

    std::vector<SessionDate> market_dates;
    std::vector<SessionDate> signal_dates;
    std::vector<SignalDirection> signal_directions;

    std::vector<double> cash_at_fill;
    std::vector<PositionQuantity> position_at_fill;
    std::vector<std::size_t> snapshots_before_fill;
};

// Records an event that has already been processed by BacktestEngine::step().
//
// This helper is reused for ordinary completion, interleaved execution,
// and prefix-only execution in Task 4.6.4.
void record_processed_event(
    Trace& trace,
    const Event& value,
    const BacktestEngine& engine
) {
    if (std::holds_alternative<MarketEvent>(value)) {
        const auto& market_event =
            std::get<MarketEvent>(value);

        trace.kinds.push_back(EventKind::Market);

        trace.market_dates.push_back(
            market_event.bar().session_date()
        );

    } else if (std::holds_alternative<SignalEvent>(value)) {
        const auto& signal =
            std::get<SignalEvent>(value);

        trace.kinds.push_back(EventKind::Signal);

        REQUIRE(signal.symbol() == "AAPL");
        REQUIRE(signal.direction() != SignalDirection::Short);

        trace.signal_dates.push_back(
            signal.signal_date()
        );

        trace.signal_directions.push_back(
            signal.direction()
        );

    } else if (std::holds_alternative<OrderEvent>(value)) {
        trace.kinds.push_back(EventKind::Order);

    } else if (std::holds_alternative<FillEvent>(value)) {
        trace.kinds.push_back(EventKind::Fill);

        // step() has already applied the fill to Portfolio.
        trace.cash_at_fill.push_back(
            engine.portfolio().cash()
        );

        trace.position_at_fill.push_back(
            engine.portfolio().position_quantity()
        );

        trace.snapshots_before_fill.push_back(
            engine.portfolio().snapshots().size()
        );

    } else {
        FAIL("Unexpected event type");
    }
}

Trace run_to_completion(BacktestEngine& engine) {
    Trace trace;

    while (const auto event = engine.step()) {
        record_processed_event(trace, *event, engine);
    }

    REQUIRE(engine.finished());
    REQUIRE_FALSE(engine.step().has_value());

    return trace;
}

void require_near(
    double actual,
    double expected,
    double tolerance = 1e-10
) {
    REQUIRE(std::isfinite(actual));
    REQUIRE(std::abs(actual - expected) <= tolerance);
}

// =============================================================================
// Task 4.6.1 — End-to-End Strategy Execution
// =============================================================================

struct ExpectedSnapshot final {
    double cash;
    PositionQuantity shares;
    double market_value;
    double equity;
};

constexpr std::array<ExpectedSnapshot, 11> expected_snapshots{{
    {1'000.0,  0,    0.0, 1'000.0},
    {1'000.0,  0,    0.0, 1'000.0},
    {1'000.0,  0,    0.0, 1'000.0},
    {   82.0,  9,  918.0, 1'000.0},
    {   82.0,  9,  945.0, 1'027.0},
    {1'018.0,  0,    0.0, 1'018.0},
    {1'018.0,  0,    0.0, 1'018.0},
    {   38.0, 10,  980.0, 1'018.0},
    {   38.0, 10, 1'030.0, 1'068.0},
    {   38.0, 10, 1'050.0, 1'088.0},
    {1'048.0,  0,    0.0, 1'048.0}
}};

}  // namespace

// -----------------------------------------------------------------------------
// Test 1: RSI causal lifecycle
// -----------------------------------------------------------------------------

TEST_CASE(
    "StrategyIntegration RSI end-to-end has exact causal event order and fills"
) {
    const auto bars = oracle_bars();

    auto engine = make_rsi_engine(bars);
    const auto trace = run_to_completion(engine);

    const std::vector<EventKind> expected_kinds{
        EventKind::Market,
        EventKind::Market,

        EventKind::Market,
        EventKind::Signal,
        EventKind::Order,

        EventKind::Fill,
        EventKind::Market,

        EventKind::Market,
        EventKind::Signal,
        EventKind::Order,

        EventKind::Fill,
        EventKind::Market,

        EventKind::Market,
        EventKind::Signal,
        EventKind::Order,

        EventKind::Fill,
        EventKind::Market,

        EventKind::Market,

        EventKind::Market,
        EventKind::Signal,
        EventKind::Order,

        EventKind::Fill,
        EventKind::Market
    };

    REQUIRE(trace.kinds == expected_kinds);

    REQUIRE(trace.market_dates.size() == bars.size());

    for (std::size_t i = 0; i < bars.size(); ++i) {
        REQUIRE(
            trace.market_dates[i] == bars[i].session_date()
        );
    }

    REQUIRE(
        trace.signal_dates == std::vector<SessionDate>{
            session_date(2),
            session_date(4),
            session_date(6),
            session_date(9)
        }
    );

    REQUIRE(
        trace.signal_directions == std::vector<SignalDirection>{
            SignalDirection::Long,
            SignalDirection::Flat,
            SignalDirection::Long,
            SignalDirection::Flat
        }
    );

    // First Long signal at Close(2)=98.
    // Execution at Open(3)=102 can afford only 9 shares.
    REQUIRE(
        trace.position_at_fill == std::vector<PositionQuantity>{
            9, 0, 10, 0
        }
    );

    REQUIRE(
        trace.cash_at_fill == std::vector<double>{
            82.0, 1'018.0, 38.0, 1'048.0
        }
    );

    REQUIRE(
        trace.snapshots_before_fill == std::vector<std::size_t>{
            3, 5, 7, 10
        }
    );

    const auto& snapshots = engine.portfolio().snapshots();

    REQUIRE(snapshots.size() == expected_snapshots.size());

    for (std::size_t i = 0; i < expected_snapshots.size(); ++i) {
        const auto& expected = expected_snapshots[i];
        const auto& actual = snapshots[i];

        REQUIRE(actual.session_date() == session_date(i));

        require_near(actual.cash(), expected.cash);

        REQUIRE(
            actual.position_quantity() == expected.shares
        );

        require_near(
            actual.market_value(),
            expected.market_value
        );

        require_near(
            actual.equity(),
            expected.equity
        );
    }

    REQUIRE(engine.portfolio().position_quantity() == 0);

    require_near(
        engine.portfolio().cash(),
        1'048.0
    );

    require_near(
        engine.portfolio().latest_equity(),
        1'048.0
    );
}

// -----------------------------------------------------------------------------
// Test 2: RSI returns and absolute performance
// -----------------------------------------------------------------------------

TEST_CASE(
    "StrategyIntegration RSI engine snapshots flow into returns and absolute metrics"
) {
    auto engine = make_rsi_engine(oracle_bars());

    const auto trace = run_to_completion(engine);

    REQUIRE(
        trace.market_dates.size() == oracle_prices.size()
    );

    const auto& snapshots = engine.portfolio().snapshots();

    const auto returns = build_return_series(
        initial_capital,
        snapshots
    );

    const std::array<double, 11> expected_simple_returns{
        0.0,
        0.0,
        0.0,
        0.0,
        27.0 / 1'000.0,
        -9.0 / 1'027.0,
        0.0,
        0.0,
        50.0 / 1'018.0,
        20.0 / 1'068.0,
        -40.0 / 1'088.0
    };

    REQUIRE(
        returns.size() == expected_simple_returns.size()
    );

    for (std::size_t i = 0; i < returns.size(); ++i) {
        REQUIRE(
            returns[i].session_date == session_date(i)
        );

        REQUIRE(returns[i].simple_return.has_value());
        REQUIRE(returns[i].log_return.has_value());

        require_near(
            *returns[i].simple_return,
            expected_simple_returns[i]
        );

        require_near(
            *returns[i].log_return,
            std::log1p(expected_simple_returns[i])
        );
    }

    const auto metrics = calculate_performance_metrics(
        initial_capital,
        snapshots,
        returns,
        annualization_factor,
        0.0
    );

    REQUIRE(metrics.total_return.has_value());
    REQUIRE(metrics.cagr.has_value());
    REQUIRE(metrics.annualized_volatility.has_value());
    REQUIRE(metrics.sharpe_ratio.has_value());
    REQUIRE(metrics.maximum_drawdown.has_value());

    require_near(
        *metrics.total_return,
        0.048
    );

    require_near(
        *metrics.cagr,
        1.9272409716555217
    );

    require_near(
        *metrics.annualized_volatility,
        0.3441297458182024
    );

    require_near(
        *metrics.sharpe_ratio,
        3.2829138913683935
    );

    require_near(
        *metrics.maximum_drawdown,
        40.0 / 1'088.0
    );
}

// -----------------------------------------------------------------------------
// Test 3: Next-open gap
// -----------------------------------------------------------------------------

TEST_CASE(
    "StrategyIntegration RSI next-open gap changes fills without changing signals"
) {
    auto baseline = make_rsi_engine(oracle_bars());

    auto gapped = make_rsi_engine(
        oracle_bars(true)
    );

    const auto baseline_trace = run_to_completion(baseline);
    const auto gapped_trace = run_to_completion(gapped);

    REQUIRE(
        gapped_trace.signal_dates ==
        baseline_trace.signal_dates
    );

    REQUIRE(
        gapped_trace.signal_directions ==
        baseline_trace.signal_directions
    );

    REQUIRE(
        gapped_trace.market_dates ==
        baseline_trace.market_dates
    );

    REQUIRE(
        gapped_trace.kinds ==
        baseline_trace.kinds
    );

    // Open(3)=120 allows 8 shares rather than 9.
    REQUIRE(
        gapped_trace.position_at_fill ==
        std::vector<PositionQuantity>{8, 0, 8, 0}
    );

    REQUIRE(
        gapped_trace.cash_at_fill ==
        std::vector<double>{40.0, 872.0, 88.0, 896.0}
    );

    REQUIRE(
        gapped_trace.snapshots_before_fill ==
        std::vector<std::size_t>{3, 5, 7, 10}
    );

    const auto& snapshots = gapped.portfolio().snapshots();

    REQUIRE(snapshots.size() == oracle_prices.size());

    require_near(snapshots[2].equity(), 1'000.0);
    require_near(snapshots[3].cash(), 40.0);

    REQUIRE(
        snapshots[3].position_quantity() == 8
    );

    require_near(snapshots[3].equity(), 856.0);
    require_near(snapshots.back().equity(), 896.0);

    const auto returns = build_return_series(
        initial_capital,
        snapshots
    );

    const auto metrics = calculate_performance_metrics(
        initial_capital,
        snapshots,
        returns,
        annualization_factor,
        0.0
    );

    REQUIRE(metrics.total_return.has_value());

    require_near(
        *metrics.total_return,
        -0.104
    );
}

// =============================================================================
// Task 4.6.2 — Strategy Coverage
// =============================================================================

namespace {

struct ExpectedTransition final {
    std::size_t index;
    SignalDirection direction;
};

std::vector<Bar> coverage_close_bars(
    std::initializer_list<double> closes
) {
    std::vector<Bar> bars;
    bars.reserve(closes.size());

    std::size_t index = 0;

    for (const double close : closes) {
        bars.emplace_back(
            session_date(index),
            "AAPL",
            close,
            close,
            close,
            close,
            static_cast<std::uint64_t>(1'000 + index)
        );

        ++index;
    }

    return bars;
}

std::vector<Bar> coverage_channel_bars() {
    // Approved Channel Breakout oracle:
    // {High, Low, Close}.
    const std::array<std::array<double, 3>, 9> values{{
        {101.0,  98.0, 100.0},
        {104.0,  99.0, 103.0},
        {108.0, 101.0, 107.0},
        {109.0, 104.0, 107.0},
        {107.0,  97.0,  98.0},
        {103.0,  96.0, 101.0},
        {112.0, 100.0, 111.0},
        {114.0, 106.0, 112.0},
        {113.0,  93.0,  95.0}
    }};

    std::vector<Bar> bars;
    bars.reserve(values.size());

    for (std::size_t i = 0; i < values.size(); ++i) {
        const double high = values[i][0];
        const double low = values[i][1];
        const double close = values[i][2];

        bars.emplace_back(
            session_date(i),
            "AAPL",
            close,
            high,
            low,
            close,
            static_cast<std::uint64_t>(1'000 + i)
        );
    }

    return bars;
}

void require_strategy_coverage(
    const std::vector<Bar>& bars,
    std::unique_ptr<Strategy> strategy,
    std::initializer_list<ExpectedTransition> expected_transitions,
    double expected_final_equity
) {
    const std::vector<ExpectedTransition> expected{
        expected_transitions.begin(),
        expected_transitions.end()
    };

    REQUIRE_FALSE(bars.empty());
    REQUIRE(strategy != nullptr);

    for (std::size_t i = 0; i < expected.size(); ++i) {
        REQUIRE(expected[i].index < bars.size());

        if (i > 0) {
            REQUIRE(
                expected[i - 1].index < expected[i].index
            );
        }

        const bool valid_direction =
            expected[i].direction == SignalDirection::Long ||
            expected[i].direction == SignalDirection::Flat;

        REQUIRE(valid_direction);
    }

    BacktestEngine engine{
        HistoricalDataFeed{bars},
        std::move(strategy),
        Portfolio{"AAPL", initial_capital},
        PositionSizer{1.0}
    };

    const auto trace = run_to_completion(engine);

    // 1. Validate signal dates and directions.
    REQUIRE(trace.signal_dates.size() == expected.size());
    REQUIRE(trace.signal_directions.size() == expected.size());

    for (std::size_t i = 0; i < expected.size(); ++i) {
        REQUIRE(
            trace.signal_dates[i] ==
            bars[expected[i].index].session_date()
        );

        REQUIRE(
            trace.signal_directions[i] ==
            expected[i].direction
        );
    }

    REQUIRE(trace.market_dates.size() == bars.size());

    for (std::size_t i = 0; i < bars.size(); ++i) {
        REQUIRE(
            trace.market_dates[i] == bars[i].session_date()
        );
    }

    // 2. Validate causal event order.
    std::vector<EventKind> expected_kinds;
    std::size_t next_signal = 0;

    for (std::size_t i = 0; i < bars.size(); ++i) {
        if (
            next_signal > 0 &&
            expected[next_signal - 1].index + 1 == i
        ) {
            expected_kinds.push_back(EventKind::Fill);
        }

        expected_kinds.push_back(EventKind::Market);

        if (
            next_signal < expected.size() &&
            expected[next_signal].index == i
        ) {
            expected_kinds.push_back(EventKind::Signal);
            expected_kinds.push_back(EventKind::Order);
            ++next_signal;
        }
    }

    REQUIRE(next_signal == expected.size());
    REQUIRE(trace.kinds == expected_kinds);

    // 3. Reconstruct fills and Portfolio state.
    struct PendingOrder final {
        SignalDirection direction;
        PositionQuantity quantity;
    };

    const auto& snapshots = engine.portfolio().snapshots();

    REQUIRE(snapshots.size() == bars.size());

    double cash = initial_capital;
    PositionQuantity shares = 0;

    std::optional<PendingOrder> pending;

    std::size_t signal_cursor = 0;
    std::size_t fill_cursor = 0;

    for (std::size_t i = 0; i < bars.size(); ++i) {
        const double open = bars[i].open();
        const double close = bars[i].close();

        if (pending.has_value()) {
            if (
                pending->direction ==
                SignalDirection::Long
            ) {
                const auto affordable =
                    static_cast<PositionQuantity>(
                        std::floor(cash / open)
                    );

                const PositionQuantity filled = std::min(
                    pending->quantity,
                    affordable
                );

                REQUIRE(filled > 0);

                cash -= static_cast<double>(filled) * open;
                shares += filled;

            } else {
                REQUIRE(
                    pending->direction ==
                    SignalDirection::Flat
                );

                REQUIRE(shares > 0);

                cash += static_cast<double>(shares) * open;
                shares = 0;
            }

            REQUIRE(
                fill_cursor < trace.cash_at_fill.size()
            );

            REQUIRE(
                fill_cursor < trace.position_at_fill.size()
            );

            REQUIRE(
                fill_cursor < trace.snapshots_before_fill.size()
            );

            require_near(
                trace.cash_at_fill[fill_cursor],
                cash
            );

            REQUIRE(
                trace.position_at_fill[fill_cursor] == shares
            );

            REQUIRE(
                trace.snapshots_before_fill[fill_cursor] == i
            );

            ++fill_cursor;
            pending.reset();
        }

        const double market_value =
            static_cast<double>(shares) * close;

        const double equity = cash + market_value;

        const auto& snapshot = snapshots[i];

        REQUIRE(
            snapshot.session_date() ==
            bars[i].session_date()
        );

        require_near(snapshot.cash(), cash);

        REQUIRE(
            snapshot.position_quantity() == shares
        );

        require_near(
            snapshot.market_value(),
            market_value
        );

        require_near(snapshot.equity(), equity);

        // Signals are generated after the completed bar.
        if (
            signal_cursor < expected.size() &&
            expected[signal_cursor].index == i
        ) {
            const auto direction =
                expected[signal_cursor].direction;

            PositionQuantity requested_quantity = 0;

            if (direction == SignalDirection::Long) {
                REQUIRE(shares == 0);

                requested_quantity =
                    static_cast<PositionQuantity>(
                        std::floor(equity / close)
                    );

            } else {
                REQUIRE(
                    direction == SignalDirection::Flat
                );

                REQUIRE(shares > 0);
                requested_quantity = shares;
            }

            REQUIRE(requested_quantity > 0);

            pending = PendingOrder{
                direction,
                requested_quantity
            };

            ++signal_cursor;
        }
    }

    REQUIRE(signal_cursor == expected.size());

    const auto expected_fill_count =
        static_cast<std::size_t>(
            std::count_if(
                expected.begin(),
                expected.end(),
                [&](const ExpectedTransition& transition) {
                    return transition.index + 1 < bars.size();
                }
            )
        );

    REQUIRE(fill_cursor == expected_fill_count);
    REQUIRE(trace.cash_at_fill.size() == expected_fill_count);
    REQUIRE(trace.position_at_fill.size() == expected_fill_count);
    REQUIRE(trace.snapshots_before_fill.size() == expected_fill_count);

    // 4. Validate final equity and returns.
    const double reconstructed_final_equity =
        cash +
        static_cast<double>(shares) * bars.back().close();

    require_near(
        reconstructed_final_equity,
        expected_final_equity
    );

    require_near(
        engine.portfolio().latest_equity(),
        expected_final_equity
    );

    require_near(engine.portfolio().cash(), cash);

    REQUIRE(
        engine.portfolio().position_quantity() == shares
    );

    const auto returns = build_return_series(
        initial_capital,
        snapshots
    );

    REQUIRE(returns.size() == bars.size());

    for (std::size_t i = 0; i < returns.size(); ++i) {
        REQUIRE(
            returns[i].session_date ==
            bars[i].session_date()
        );

        REQUIRE(returns[i].simple_return.has_value());
        REQUIRE(returns[i].log_return.has_value());
    }

    const auto metrics = calculate_performance_metrics(
        initial_capital,
        snapshots,
        returns,
        4.0,
        0.0
    );

    REQUIRE(metrics.total_return.has_value());
    REQUIRE(metrics.maximum_drawdown.has_value());

    require_near(
        *metrics.total_return,
        expected_final_equity / initial_capital - 1.0
    );
}

}  // namespace

// -----------------------------------------------------------------------------
// Test 4: SMA Crossover
// -----------------------------------------------------------------------------

TEST_CASE(
    "StrategyIntegration SMA crossover executes through BacktestEngine"
) {
    require_strategy_coverage(
        coverage_close_bars({
            10, 11, 12, 10, 9, 13, 13
        }),
        std::make_unique<
            backtester::strategy::SMACrossoverStrategy
        >(
            "AAPL", 2, 3
        ),
        {
            {2, SignalDirection::Long},
            {4, SignalDirection::Flat},
            {5, SignalDirection::Long}
        },
        1'249.0
    );
}

// -----------------------------------------------------------------------------
// Test 5: EMA Crossover
// -----------------------------------------------------------------------------

TEST_CASE(
    "StrategyIntegration EMA crossover executes through BacktestEngine"
) {
    require_strategy_coverage(
        coverage_close_bars({
            10, 11, 12, 10, 9, 13, 13
        }),
        std::make_unique<
            backtester::strategy::EMACrossoverStrategy
        >(
            "AAPL", 2, 3
        ),
        {
            {2, SignalDirection::Long},
            {4, SignalDirection::Flat},
            {5, SignalDirection::Long}
        },
        1'249.0
    );
}

// -----------------------------------------------------------------------------
// Test 6: Price Filter
// -----------------------------------------------------------------------------

TEST_CASE(
    "StrategyIntegration Price Filter executes historical-only signals"
) {
    require_strategy_coverage(
        coverage_close_bars({
            100, 103, 104, 101, 98,
            99, 102, 100, 98, 95
        }),
        std::make_unique<
            backtester::strategy::PriceFilterStrategy
        >(
            "AAPL", 1, 0.02
        ),
        {
            {1, SignalDirection::Long},
            {3, SignalDirection::Flat},
            {6, SignalDirection::Long},
            {9, SignalDirection::Flat}
        },
        901.0
    );
}

// -----------------------------------------------------------------------------
// Test 7: Channel Breakout
// -----------------------------------------------------------------------------

TEST_CASE(
    "StrategyIntegration Channel Breakout executes historical channels"
) {
    require_strategy_coverage(
        coverage_channel_bars(),
        std::make_unique<
            backtester::strategy::ChannelBreakoutStrategy
        >(
            "AAPL", 2, 2, 0.0
        ),
        {
            {2, SignalDirection::Long},
            {4, SignalDirection::Flat},
            {6, SignalDirection::Long},
            {8, SignalDirection::Flat}
        },
        810.0
    );
}

// -----------------------------------------------------------------------------
// Test 8: Momentum
// -----------------------------------------------------------------------------

TEST_CASE(
    "StrategyIntegration Momentum executes through BacktestEngine"
) {
    require_strategy_coverage(
        coverage_close_bars({
            100, 101, 102, 104, 99, 100,
            101, 105, 103, 97, 101
        }),
        std::make_unique<
            backtester::strategy::MomentumStrategy
        >(
            "AAPL", 2
        ),
        {
            {2, SignalDirection::Long},
            {4, SignalDirection::Flat},
            {6, SignalDirection::Long},
            {9, SignalDirection::Flat}
        },
        928.0
    );
}

// -----------------------------------------------------------------------------
// Test 9: MACD Crossover
// -----------------------------------------------------------------------------

TEST_CASE(
    "StrategyIntegration MACD crossover executes through BacktestEngine"
) {
    require_strategy_coverage(
        coverage_close_bars({
            10, 12, 11, 15, 13,
            16, 16, 11, 11, 17
        }),
        std::make_unique<
            backtester::strategy::MACDCrossoverStrategy
        >(
            "AAPL", 2, 3, 2
        ),
        {
            {3, SignalDirection::Long},
            {4, SignalDirection::Flat},
            {5, SignalDirection::Long},
            {6, SignalDirection::Flat},
            {9, SignalDirection::Long}
        },
        828.0
    );
}

// =============================================================================
// Task 4.6.3 — Performance and Benchmark Integration
// =============================================================================

namespace {

constexpr double comparison_annualization = 4.0;
constexpr double comparison_risk_free_rate = 0.04;

struct ExpectedRelativeMetrics final {
    double excess_total_return;
    double annualized_return_difference;
    double tracking_error;
    double information_ratio;
    double correlation;
    double beta;
    double jensen_alpha;
};

BacktestEngine make_comparison_benchmark(
    const std::vector<Bar>& bars
) {
    return BacktestEngine{
        HistoricalDataFeed{bars},
        std::make_unique<BuyAndHoldStrategy>("AAPL"),
        Portfolio{"AAPL", initial_capital},
        PositionSizer{1.0}
    };
}

void require_relative_value(
    const std::optional<double>& actual,
    double expected
) {
    REQUIRE(actual.has_value());
    require_near(*actual, expected);
}

void require_complete_comparison(
    const BacktestEngine& strategy_engine,
    const BacktestEngine& benchmark_engine,
    std::span<const double> expected_strategy_equity,
    std::span<const double> expected_benchmark_equity,
    const ExpectedRelativeMetrics& expected
) {
    const auto& strategy_snapshots =
        strategy_engine.portfolio().snapshots();

    const auto& benchmark_snapshots =
        benchmark_engine.portfolio().snapshots();

    const std::size_t count =
        expected_strategy_equity.size();

    REQUIRE(count >= 2);
    REQUIRE(expected_benchmark_equity.size() == count);
    REQUIRE(strategy_snapshots.size() == count);
    REQUIRE(benchmark_snapshots.size() == count);

    // Full-horizon date alignment and equity paths.
    for (std::size_t i = 0; i < count; ++i) {
        REQUIRE(
            strategy_snapshots[i].session_date() ==
            benchmark_snapshots[i].session_date()
        );

        require_near(
            strategy_snapshots[i].equity(),
            expected_strategy_equity[i]
        );

        require_near(
            benchmark_snapshots[i].equity(),
            expected_benchmark_equity[i]
        );
    }

    const auto strategy_returns = build_return_series(
        initial_capital,
        strategy_snapshots
    );

    const auto benchmark_returns = build_return_series(
        initial_capital,
        benchmark_snapshots
    );

    const auto active_returns = build_active_return_series(
        strategy_returns,
        benchmark_returns
    );

    REQUIRE(strategy_returns.size() == count);
    REQUIRE(benchmark_returns.size() == count);
    REQUIRE(active_returns.size() == count);

    for (std::size_t i = 0; i < count; ++i) {
        const double previous_strategy_equity =
            i == 0
                ? initial_capital
                : expected_strategy_equity[i - 1];

        const double previous_benchmark_equity =
            i == 0
                ? initial_capital
                : expected_benchmark_equity[i - 1];

        const double expected_strategy_return =
            expected_strategy_equity[i] /
            previous_strategy_equity - 1.0;

        const double expected_benchmark_return =
            expected_benchmark_equity[i] /
            previous_benchmark_equity - 1.0;

        const double expected_active_return =
            expected_strategy_return -
            expected_benchmark_return;

        REQUIRE(
            strategy_returns[i].session_date ==
            benchmark_returns[i].session_date
        );

        REQUIRE(
            active_returns[i].session_date ==
            strategy_returns[i].session_date
        );

        REQUIRE(strategy_returns[i].simple_return.has_value());
        REQUIRE(benchmark_returns[i].simple_return.has_value());
        REQUIRE(active_returns[i].active_return.has_value());

        require_near(
            *strategy_returns[i].simple_return,
            expected_strategy_return
        );

        require_near(
            *benchmark_returns[i].simple_return,
            expected_benchmark_return
        );

        require_near(
            *active_returns[i].active_return,
            expected_active_return
        );
    }

    // Independent absolute metrics.
    const auto strategy_metrics =
        calculate_performance_metrics(
            initial_capital,
            strategy_snapshots,
            strategy_returns,
            comparison_annualization,
            comparison_risk_free_rate
        );

    const auto benchmark_metrics =
        calculate_performance_metrics(
            initial_capital,
            benchmark_snapshots,
            benchmark_returns,
            comparison_annualization,
            comparison_risk_free_rate
        );

    REQUIRE(strategy_metrics.total_return.has_value());
    REQUIRE(benchmark_metrics.total_return.has_value());
    REQUIRE(strategy_metrics.cagr.has_value());
    REQUIRE(benchmark_metrics.cagr.has_value());
    REQUIRE(strategy_metrics.annualized_volatility.has_value());
    REQUIRE(benchmark_metrics.annualized_volatility.has_value());
    REQUIRE(strategy_metrics.maximum_drawdown.has_value());
    REQUIRE(benchmark_metrics.maximum_drawdown.has_value());

    require_near(
        *strategy_metrics.total_return,
        expected_strategy_equity.back() /
        initial_capital - 1.0
    );

    require_near(
        *benchmark_metrics.total_return,
        expected_benchmark_equity.back() /
        initial_capital - 1.0
    );

    // Seven relative metrics.
    const auto relative =
        calculate_relative_performance_metrics(
            strategy_metrics,
            benchmark_metrics,
            strategy_returns,
            benchmark_returns,
            comparison_annualization,
            comparison_risk_free_rate
        );

    require_relative_value(
        relative.excess_total_return,
        expected.excess_total_return
    );

    require_relative_value(
        relative.annualized_return_difference,
        expected.annualized_return_difference
    );

    require_relative_value(
        relative.tracking_error,
        expected.tracking_error
    );

    require_relative_value(
        relative.information_ratio,
        expected.information_ratio
    );

    require_relative_value(
        relative.correlation,
        expected.correlation
    );

    require_relative_value(
        relative.beta,
        expected.beta
    );

    require_relative_value(
        relative.jensen_alpha,
        expected.jensen_alpha
    );

    // Post-processing must not mutate completed engines.
    REQUIRE(
        strategy_engine.portfolio().snapshots().size() == count
    );

    REQUIRE(
        benchmark_engine.portfolio().snapshots().size() == count
    );

    require_near(
        strategy_engine.portfolio().latest_equity(),
        expected_strategy_equity.back()
    );

    require_near(
        benchmark_engine.portfolio().latest_equity(),
        expected_benchmark_equity.back()
    );
}

}  // namespace

// -----------------------------------------------------------------------------
// Test 10: RSI versus Buy & Hold
// -----------------------------------------------------------------------------

TEST_CASE(
    "StrategyIntegration RSI compares full performance against Buy and Hold"
) {
    const auto bars = oracle_bars();

    auto strategy_engine = make_rsi_engine(bars);
    auto benchmark_engine = make_comparison_benchmark(bars);

    const auto strategy_trace =
        run_to_completion(strategy_engine);

    const auto benchmark_trace =
        run_to_completion(benchmark_engine);

    REQUIRE(
        strategy_trace.signal_dates ==
        std::vector<SessionDate>{
            session_date(2),
            session_date(4),
            session_date(6),
            session_date(9)
        }
    );

    REQUIRE(strategy_trace.position_at_fill.size() == 4);

    REQUIRE(
        benchmark_trace.signal_dates ==
        std::vector<SessionDate>{session_date(0)}
    );

    REQUIRE(
        benchmark_trace.signal_directions ==
        std::vector<SignalDirection>{SignalDirection::Long}
    );

    REQUIRE(
        benchmark_trace.position_at_fill ==
        std::vector<PositionQuantity>{10}
    );

    REQUIRE(
        benchmark_trace.cash_at_fill ==
        std::vector<double>{10.0}
    );

    REQUIRE(
        strategy_trace.market_dates ==
        benchmark_trace.market_dates
    );

    constexpr std::array<double, 11> strategy_equity{
        1'000.0,
        1'000.0,
        1'000.0,
        1'000.0,
        1'027.0,
        1'018.0,
        1'018.0,
        1'018.0,
        1'068.0,
        1'088.0,
        1'048.0
    };

    constexpr std::array<double, 11> benchmark_equity{
        1'000.0,
        1'000.0,
        990.0,
        1'030.0,
        1'060.0,
        1'050.0,
        1'010.0,
        990.0,
        1'040.0,
        1'060.0,
        1'020.0
    };

    const ExpectedRelativeMetrics expected{
        .excess_total_return =
            0.028,

        .annualized_return_difference =
            0.009967788474257055,

        .tracking_error =
            0.03755851833484551,

        .information_ratio =
            0.2431634968976169,

        .correlation =
            0.7755498836105996,

        .beta =
            0.5666026644873932,

        .jensen_alpha =
            -0.004135154013141386
    };

    require_complete_comparison(
        strategy_engine,
        benchmark_engine,
        strategy_equity,
        benchmark_equity,
        expected
    );
}

// -----------------------------------------------------------------------------
// Test 11: SMA Crossover versus Buy & Hold
// -----------------------------------------------------------------------------

TEST_CASE(
    "StrategyIntegration SMA crossover compares independent benchmark results"
) {
    const auto bars = coverage_close_bars({
        10, 11, 12, 10, 9, 13, 13
    });

    BacktestEngine strategy_engine{
        HistoricalDataFeed{bars},
        std::make_unique<
            backtester::strategy::SMACrossoverStrategy
        >(
            "AAPL", 2, 3
        ),
        Portfolio{"AAPL", initial_capital},
        PositionSizer{1.0}
    };

    auto benchmark_engine = make_comparison_benchmark(bars);

    const auto strategy_trace =
        run_to_completion(strategy_engine);

    const auto benchmark_trace =
        run_to_completion(benchmark_engine);

    REQUIRE(
        strategy_trace.signal_dates ==
        std::vector<SessionDate>{
            session_date(2),
            session_date(4),
            session_date(5)
        }
    );

    REQUIRE(
        strategy_trace.signal_directions ==
        std::vector<SignalDirection>{
            SignalDirection::Long,
            SignalDirection::Flat,
            SignalDirection::Long
        }
    );

    REQUIRE(
        strategy_trace.position_at_fill ==
        std::vector<PositionQuantity>{83, 0, 96}
    );

    REQUIRE(
        strategy_trace.cash_at_fill ==
        std::vector<double>{170.0, 1'249.0, 1.0}
    );

    REQUIRE(
        benchmark_trace.signal_dates ==
        std::vector<SessionDate>{session_date(0)}
    );

    REQUIRE(
        benchmark_trace.position_at_fill ==
        std::vector<PositionQuantity>{90}
    );

    REQUIRE(
        benchmark_trace.cash_at_fill ==
        std::vector<double>{10.0}
    );

    REQUIRE(
        strategy_trace.market_dates ==
        benchmark_trace.market_dates
    );

    constexpr std::array<double, 7> strategy_equity{
        1'000.0,
        1'000.0,
        1'000.0,
        1'000.0,
        917.0,
        1'249.0,
        1'249.0
    };

    constexpr std::array<double, 7> benchmark_equity{
        1'000.0,
        1'000.0,
        1'090.0,
        910.0,
        820.0,
        1'180.0,
        1'180.0
    };

    const ExpectedRelativeMetrics expected{
        .excess_total_return =
            0.069,

        .annualized_return_difference =
            0.03628076598126717,

        .tracking_error =
            0.16637317443496888,

        .information_ratio =
            0.04830616288865431,

        .correlation =
            0.9210888018119706,

        .beta =
            0.6872288356314108,

        .jensen_alpha =
            0.0430693345536675
    };

    require_complete_comparison(
        strategy_engine,
        benchmark_engine,
        strategy_equity,
        benchmark_equity,
        expected
    );
}

// =============================================================================
// Task 4.6.4 — Validation & Closure
// =============================================================================

namespace {

// -----------------------------------------------------------------------------
// Deterministic trace and Portfolio comparison
// -----------------------------------------------------------------------------

void require_equal_traces(
    const Trace& actual,
    const Trace& expected
) {
    REQUIRE(actual.kinds == expected.kinds);

    REQUIRE(
        actual.market_dates == expected.market_dates
    );

    REQUIRE(
        actual.signal_dates == expected.signal_dates
    );

    REQUIRE(
        actual.signal_directions == expected.signal_directions
    );

    REQUIRE(
        actual.cash_at_fill == expected.cash_at_fill
    );

    REQUIRE(
        actual.position_at_fill == expected.position_at_fill
    );

    REQUIRE(
        actual.snapshots_before_fill ==
        expected.snapshots_before_fill
    );
}

void require_equal_snapshot(
    const PortfolioSnapshot& actual,
    const PortfolioSnapshot& expected
) {
    REQUIRE(
        actual.session_date() == expected.session_date()
    );

    REQUIRE(actual.cash() == expected.cash());

    REQUIRE(
        actual.position_quantity() ==
        expected.position_quantity()
    );

    REQUIRE(
        actual.market_value() == expected.market_value()
    );

    REQUIRE(
        actual.equity() == expected.equity()
    );
}

void require_equal_portfolios(
    const BacktestEngine& actual,
    const BacktestEngine& expected
) {
    const auto& actual_portfolio = actual.portfolio();
    const auto& expected_portfolio = expected.portfolio();

    REQUIRE(
        actual_portfolio.symbol() ==
        expected_portfolio.symbol()
    );

    REQUIRE(
        actual_portfolio.initial_capital() ==
        expected_portfolio.initial_capital()
    );

    REQUIRE(
        actual_portfolio.cash() ==
        expected_portfolio.cash()
    );

    REQUIRE(
        actual_portfolio.position_quantity() ==
        expected_portfolio.position_quantity()
    );

    REQUIRE(
        actual_portfolio.latest_mark_price() ==
        expected_portfolio.latest_mark_price()
    );

    REQUIRE(
        actual_portfolio.latest_market_value() ==
        expected_portfolio.latest_market_value()
    );

    REQUIRE(
        actual_portfolio.latest_equity() ==
        expected_portfolio.latest_equity()
    );

    const auto& actual_snapshots =
        actual_portfolio.snapshots();

    const auto& expected_snapshots =
        expected_portfolio.snapshots();

    REQUIRE(
        actual_snapshots.size() ==
        expected_snapshots.size()
    );

    for (std::size_t i = 0;
         i < actual_snapshots.size();
         ++i) {
        require_equal_snapshot(
            actual_snapshots[i],
            expected_snapshots[i]
        );
    }
}

void require_equal_performance(
    const BacktestEngine& actual,
    const BacktestEngine& expected
) {
    const auto actual_returns = build_return_series(
        initial_capital,
        actual.portfolio().snapshots()
    );

    const auto expected_returns = build_return_series(
        initial_capital,
        expected.portfolio().snapshots()
    );

    REQUIRE(
        actual_returns.size() ==
        expected_returns.size()
    );

    for (std::size_t i = 0; i < actual_returns.size(); ++i) {
        REQUIRE(
            actual_returns[i].session_date ==
            expected_returns[i].session_date
        );

        REQUIRE(
            actual_returns[i].simple_return ==
            expected_returns[i].simple_return
        );

        REQUIRE(
            actual_returns[i].log_return ==
            expected_returns[i].log_return
        );
    }

    const auto actual_metrics = calculate_performance_metrics(
        initial_capital,
        actual.portfolio().snapshots(),
        actual_returns,
        comparison_annualization,
        comparison_risk_free_rate
    );

    const auto expected_metrics = calculate_performance_metrics(
        initial_capital,
        expected.portfolio().snapshots(),
        expected_returns,
        comparison_annualization,
        comparison_risk_free_rate
    );

    REQUIRE(
        actual_metrics.total_return ==
        expected_metrics.total_return
    );

    REQUIRE(
        actual_metrics.cagr ==
        expected_metrics.cagr
    );

    REQUIRE(
        actual_metrics.annualized_volatility ==
        expected_metrics.annualized_volatility
    );

    REQUIRE(
        actual_metrics.sharpe_ratio ==
        expected_metrics.sharpe_ratio
    );

    REQUIRE(
        actual_metrics.maximum_drawdown ==
        expected_metrics.maximum_drawdown
    );
}

void require_equal_relative_results(
    const BacktestEngine& actual,
    const BacktestEngine& expected,
    const BacktestEngine& benchmark
) {
    const auto actual_returns = build_return_series(
        initial_capital,
        actual.portfolio().snapshots()
    );

    const auto expected_returns = build_return_series(
        initial_capital,
        expected.portfolio().snapshots()
    );

    const auto benchmark_returns = build_return_series(
        initial_capital,
        benchmark.portfolio().snapshots()
    );

    const auto actual_metrics = calculate_performance_metrics(
        initial_capital,
        actual.portfolio().snapshots(),
        actual_returns,
        comparison_annualization,
        comparison_risk_free_rate
    );

    const auto expected_metrics = calculate_performance_metrics(
        initial_capital,
        expected.portfolio().snapshots(),
        expected_returns,
        comparison_annualization,
        comparison_risk_free_rate
    );

    const auto benchmark_metrics = calculate_performance_metrics(
        initial_capital,
        benchmark.portfolio().snapshots(),
        benchmark_returns,
        comparison_annualization,
        comparison_risk_free_rate
    );

    const auto actual_relative = calculate_relative_performance_metrics(
        actual_metrics,
        benchmark_metrics,
        actual_returns,
        benchmark_returns,
        comparison_annualization,
        comparison_risk_free_rate
    );

    const auto expected_relative = calculate_relative_performance_metrics(
        expected_metrics,
        benchmark_metrics,
        expected_returns,
        benchmark_returns,
        comparison_annualization,
        comparison_risk_free_rate
    );

    REQUIRE(
        actual_relative.excess_total_return ==
        expected_relative.excess_total_return
    );

    REQUIRE(
        actual_relative.annualized_return_difference ==
        expected_relative.annualized_return_difference
    );

    REQUIRE(
        actual_relative.tracking_error ==
        expected_relative.tracking_error
    );

    REQUIRE(
        actual_relative.information_ratio ==
        expected_relative.information_ratio
    );

    REQUIRE(
        actual_relative.correlation ==
        expected_relative.correlation
    );

    REQUIRE(
        actual_relative.beta ==
        expected_relative.beta
    );

    REQUIRE(
        actual_relative.jensen_alpha ==
        expected_relative.jensen_alpha
    );
}

// -----------------------------------------------------------------------------
// Advance exactly to a completed snapshot boundary.
//
// This is important for checking future-data independence without
// accidentally loading a future MarketEvent or executing its pending fill.
// -----------------------------------------------------------------------------

void run_until_snapshot_count(
    BacktestEngine& engine,
    std::size_t snapshot_count,
    Trace& trace
) {
    REQUIRE(
        engine.portfolio().snapshots().size() <=
        snapshot_count
    );

    while (
        engine.portfolio().snapshots().size() <
        snapshot_count
    ) {
        const auto event = engine.step();

        REQUIRE(event.has_value());

        record_processed_event(trace, *event, engine);
    }

    REQUIRE(
        engine.portfolio().snapshots().size() ==
        snapshot_count
    );
}

}  // namespace

// -----------------------------------------------------------------------------
// Test 12: Deterministic Replay
// -----------------------------------------------------------------------------

TEST_CASE(
    "StrategyIntegration deterministic replay reproduces events snapshots and metrics"
) {
    const auto bars = oracle_bars();

    auto first = make_rsi_engine(bars);
    auto second = make_rsi_engine(bars);

    const auto first_trace = run_to_completion(first);
    const auto second_trace = run_to_completion(second);

    // Identical inputs must produce identical observable event traces.
    require_equal_traces(first_trace, second_trace);

    // Portfolio accounting and every historical valuation must match.
    require_equal_portfolios(first, second);

    // Absolute performance must be exactly reproducible.
    require_equal_performance(first, second);

    // Relative performance must also be reproducible.
    auto benchmark = make_comparison_benchmark(bars);

    const auto benchmark_trace =
        run_to_completion(benchmark);

    REQUIRE(benchmark_trace.market_dates == first_trace.market_dates);

    require_equal_relative_results(
        first,
        second,
        benchmark
    );

    // A deterministic replay must preserve the known end state.
    REQUIRE(first.portfolio().position_quantity() == 0);
    REQUIRE(second.portfolio().position_quantity() == 0);

    require_near(
        first.portfolio().latest_equity(),
        1'048.0
    );

    require_near(
        second.portfolio().latest_equity(),
        1'048.0
    );
}

// -----------------------------------------------------------------------------
// Test 13: Engine State Isolation
// -----------------------------------------------------------------------------

TEST_CASE(
    "StrategyIntegration interleaved engines preserve independent state"
) {
    const auto rsi_bars = oracle_bars();

    const auto sma_bars = coverage_close_bars({
        10, 11, 12, 10, 9, 13, 13
    });

    // Reference runs are completed independently.
    auto reference_rsi = make_rsi_engine(rsi_bars);

    BacktestEngine reference_sma{
        HistoricalDataFeed{sma_bars},
        std::make_unique<
            backtester::strategy::SMACrossoverStrategy
        >(
            "AAPL", 2, 3
        ),
        Portfolio{"AAPL", initial_capital},
        PositionSizer{1.0}
    };

    const auto reference_rsi_trace =
        run_to_completion(reference_rsi);

    const auto reference_sma_trace =
        run_to_completion(reference_sma);

    // Construct fresh engines with independent mutable state.
    auto interleaved_rsi = make_rsi_engine(rsi_bars);

    BacktestEngine interleaved_sma{
        HistoricalDataFeed{sma_bars},
        std::make_unique<
            backtester::strategy::SMACrossoverStrategy
        >(
            "AAPL", 2, 3
        ),
        Portfolio{"AAPL", initial_capital},
        PositionSizer{1.0}
    };

    Trace interleaved_rsi_trace;
    Trace interleaved_sma_trace;

    bool rsi_running = true;
    bool sma_running = true;

    // Advance RSI by one event and SMA by two events per cycle.
    // This intentionally gives the engines different progress rates.
    while (rsi_running || sma_running) {
        if (rsi_running) {
            const auto event = interleaved_rsi.step();

            if (event.has_value()) {
                record_processed_event(
                    interleaved_rsi_trace,
                    *event,
                    interleaved_rsi
                );
            } else {
                rsi_running = false;
            }
        }

        for (int i = 0; i < 2 && sma_running; ++i) {
            const auto event = interleaved_sma.step();

            if (event.has_value()) {
                record_processed_event(
                    interleaved_sma_trace,
                    *event,
                    interleaved_sma
                );
            } else {
                sma_running = false;
            }
        }
    }

    REQUIRE(interleaved_rsi.finished());
    REQUIRE(interleaved_sma.finished());

    // Neither engine should observe, overwrite, or depend on the
    // other's internal indicator, strategy, event, or Portfolio state.
    require_equal_traces(
        interleaved_rsi_trace,
        reference_rsi_trace
    );

    require_equal_traces(
        interleaved_sma_trace,
        reference_sma_trace
    );

    require_equal_portfolios(
        interleaved_rsi,
        reference_rsi
    );

    require_equal_portfolios(
        interleaved_sma,
        reference_sma
    );

    require_equal_performance(
        interleaved_rsi,
        reference_rsi
    );

    require_equal_performance(
        interleaved_sma,
        reference_sma
    );

    // The two distinct strategies retain their own expected results.
    require_near(
        interleaved_rsi.portfolio().latest_equity(),
        1'048.0
    );

    require_near(
        interleaved_sma.portfolio().latest_equity(),
        1'249.0
    );
}

// -----------------------------------------------------------------------------
// Test 14: Future Data Independence
// -----------------------------------------------------------------------------

TEST_CASE(
    "StrategyIntegration completed prefix is independent of future market data"
) {
    constexpr std::size_t prefix_length = 3;

    const auto original_bars = oracle_bars();
    const auto gapped_bars = oracle_bars(true);

    // All three runs share the same first three completed bars.
    std::vector<Bar> prefix_bars{
        original_bars.begin(),
        original_bars.begin() + prefix_length
    };

    auto prefix_engine = make_rsi_engine(prefix_bars);
    auto full_engine = make_rsi_engine(original_bars);
    auto gapped_engine = make_rsi_engine(gapped_bars);

    // The truncated run observes feed exhaustion after its third bar.
    const auto prefix_trace =
        run_to_completion(prefix_engine);

    // The full runs stop immediately after completing the same prefix.
    // Neither has consumed the fourth bar yet.
    Trace full_prefix_trace;
    Trace gapped_prefix_trace;

    run_until_snapshot_count(
        full_engine,
        prefix_length,
        full_prefix_trace
    );

    run_until_snapshot_count(
        gapped_engine,
        prefix_length,
        gapped_prefix_trace
    );

    // A signal is emitted at Close(2), but its fill is not yet allowed.
    const std::vector<EventKind> expected_prefix_events{
        EventKind::Market,
        EventKind::Market,
        EventKind::Market,
        EventKind::Signal,
        EventKind::Order
    };

    REQUIRE(
        prefix_trace.kinds ==
        expected_prefix_events
    );

    REQUIRE(prefix_trace.cash_at_fill.empty());
    REQUIRE(full_prefix_trace.cash_at_fill.empty());
    REQUIRE(gapped_prefix_trace.cash_at_fill.empty());

    REQUIRE(
        prefix_trace.signal_dates ==
        std::vector<SessionDate>{session_date(2)}
    );

    REQUIRE(
        prefix_trace.signal_directions ==
        std::vector<SignalDirection>{SignalDirection::Long}
    );

    // No future market information may affect the event prefix.
    require_equal_traces(
        full_prefix_trace,
        prefix_trace
    );

    require_equal_traces(
        gapped_prefix_trace,
        prefix_trace
    );

    // Nor may it affect the completed Portfolio history.
    const auto& prefix_snapshots =
        prefix_engine.portfolio().snapshots();

    const auto& full_snapshots =
        full_engine.portfolio().snapshots();

    const auto& gapped_snapshots =
        gapped_engine.portfolio().snapshots();

    REQUIRE(prefix_snapshots.size() == prefix_length);
    REQUIRE(full_snapshots.size() == prefix_length);
    REQUIRE(gapped_snapshots.size() == prefix_length);

    for (std::size_t i = 0; i < prefix_length; ++i) {
        require_equal_snapshot(
            full_snapshots[i],
            prefix_snapshots[i]
        );

        require_equal_snapshot(
            gapped_snapshots[i],
            prefix_snapshots[i]
        );

        require_near(
            prefix_snapshots[i].equity(),
            initial_capital
        );
    }

    REQUIRE(
        full_engine.portfolio().position_quantity() == 0
    );

    REQUIRE(
        gapped_engine.portfolio().position_quantity() == 0
    );

    REQUIRE_FALSE(full_engine.finished());
    REQUIRE_FALSE(gapped_engine.finished());

    // Now allow each full run to consume its next Open.
    // The signals remain identical, but execution must respond to
    // the different future Open prices.
    const auto next_full = full_engine.step();
    const auto next_gapped = gapped_engine.step();

    REQUIRE(next_full.has_value());
    REQUIRE(next_gapped.has_value());

    REQUIRE(
        std::holds_alternative<FillEvent>(*next_full)
    );

    REQUIRE(
        std::holds_alternative<FillEvent>(*next_gapped)
    );

    // Standard run: buy 9 shares at Open=102.
    REQUIRE(
        full_engine.portfolio().position_quantity() == 9
    );

    require_near(
        full_engine.portfolio().cash(),
        82.0
    );

    // Gapped run: buy only 8 shares at Open=120.
    REQUIRE(
        gapped_engine.portfolio().position_quantity() == 8
    );

    require_near(
        gapped_engine.portfolio().cash(),
        40.0
    );

    // No new completed snapshot exists until the new session
    // has processed all its events.
    REQUIRE(
        full_engine.portfolio().snapshots().size() ==
        prefix_length
    );

    REQUIRE(
        gapped_engine.portfolio().snapshots().size() ==
        prefix_length
    );

    // Complete the remaining events.
    run_to_completion(full_engine);
    run_to_completion(gapped_engine);

    // Previously completed snapshots must remain unchanged.
    for (std::size_t i = 0; i < prefix_length; ++i) {
        require_equal_snapshot(
            full_engine.portfolio().snapshots()[i],
            prefix_snapshots[i]
        );

        require_equal_snapshot(
            gapped_engine.portfolio().snapshots()[i],
            prefix_snapshots[i]
        );
    }

    // Distinct future execution prices may affect later outcomes,
    // but never the already-completed prefix.
    require_near(
        full_engine.portfolio().latest_equity(),
        1'048.0
    );

    require_near(
        gapped_engine.portfolio().latest_equity(),
        896.0
    );
}
