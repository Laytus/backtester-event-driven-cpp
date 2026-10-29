#include "backtester/data/historical_data_feed.hpp"
#include "backtester/engine/backtest_engine.hpp"
#include "backtester/events/event.hpp"
#include "backtester/market/bar.hpp"
#include "backtester/performance/performance_metrics.hpp"
#include "backtester/performance/return_series.hpp"
#include "backtester/portfolio/portfolio.hpp"
#include "backtester/portfolio/position_sizer.hpp"
#include "backtester/strategy/buy_and_hold_strategy.hpp"
#include "backtester/strategy/strategy.hpp"
#include "backtester/performance/active_return_series.hpp"
#include "backtester/performance/relative_metrics.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
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
using backtester::market::Bar;
using backtester::market::SessionDate;
using backtester::performance::build_return_series;
using backtester::performance::calculate_performance_metrics;
using backtester::portfolio::Portfolio;
using backtester::portfolio::PortfolioSnapshot;
using backtester::portfolio::PositionSizer;
using backtester::strategy::BuyAndHoldStrategy;
using backtester::strategy::Strategy;

constexpr double initial_capital = 1'000.0;

// A test-only strategy that never requests a change in exposure.
class NoTradeStrategy final : public Strategy {
public:
    [[nodiscard]]
    std::vector<SignalEvent> on_market(const MarketEvent&) override {
        return {};
    }
};

SessionDate session_date(unsigned day) {
    return SessionDate{
        std::chrono::year{2026},
        std::chrono::month{9},
        std::chrono::day{day}
    };
}

std::vector<Bar> oracle_c_bars() {
    // These are independent input constants from the approved Oracle C.
    // OHLC envelopes are valid and trading-session dates are increasing.
    return {
        Bar{session_date(14), "AAPL", 100.0, 101.0, 99.0, 100.0, 1'000},
        Bar{session_date(15), "AAPL", 100.0, 110.0, 100.0, 110.0, 1'100},
        Bar{session_date(16), "AAPL", 115.0, 120.0, 114.0, 120.0, 1'200},
        Bar{session_date(17), "AAPL", 100.0, 101.0, 89.0, 90.0, 1'300}
    };
}

BacktestEngine make_engine(
    const std::vector<Bar>& bars,
    bool buy_and_hold,
    double capital = initial_capital
) {
    std::unique_ptr<Strategy> strategy;

    if (buy_and_hold) {
        strategy = std::make_unique<BuyAndHoldStrategy>("AAPL");
    } else {
        strategy = std::make_unique<NoTradeStrategy>();
    }

    return BacktestEngine{
        HistoricalDataFeed{bars},
        std::move(strategy),
        Portfolio{"AAPL", capital},
        PositionSizer{1.0}
    };
}

enum class EventKind {
    Market,
    Signal,
    Order,
    Fill
};

EventKind kind_of(const Event& event) {
    if (std::holds_alternative<MarketEvent>(event)) {
        return EventKind::Market;
    }
    if (std::holds_alternative<SignalEvent>(event)) {
        return EventKind::Signal;
    }
    if (std::holds_alternative<OrderEvent>(event)) {
        return EventKind::Order;
    }
    return EventKind::Fill;
}

struct RunTrace final {
    std::vector<EventKind> event_kinds;
    std::vector<SessionDate> market_dates;
    std::vector<SessionDate> signal_dates;
    std::vector<double> cash_at_fill;
    std::vector<backtester::portfolio::PositionQuantity> shares_at_fill;
};

RunTrace run_to_completion(BacktestEngine& engine) {
    RunTrace trace;

    while (const auto event = engine.step()) {
        const Event& value = *event;
        trace.event_kinds.push_back(kind_of(value));

        if (std::holds_alternative<MarketEvent>(value)) {
            trace.market_dates.push_back(
                std::get<MarketEvent>(value).bar().session_date()
            );
        } else if (std::holds_alternative<SignalEvent>(value)) {
            const auto& signal = std::get<SignalEvent>(value);
            REQUIRE(signal.symbol() == "AAPL");
            REQUIRE(signal.direction() == SignalDirection::Long);
            trace.signal_dates.push_back(signal.signal_date());
        } else if (std::holds_alternative<FillEvent>(value)) {
            // BacktestEngine::step() returns an already-processed event.
            trace.cash_at_fill.push_back(engine.portfolio().cash());
            trace.shares_at_fill.push_back(engine.portfolio().position_quantity());
        }
    }

    REQUIRE(engine.finished());
    REQUIRE_FALSE(engine.step().has_value());
    return trace;
}

std::size_t count_events(const RunTrace& trace, EventKind kind) {
    return static_cast<std::size_t>(
        std::count(trace.event_kinds.begin(), trace.event_kinds.end(), kind)
    );
}

void require_near(double actual, double expected, double tolerance = 1e-10) {
    REQUIRE(std::isfinite(actual));
    REQUIRE(std::abs(actual - expected) <= tolerance);
}

void require_snapshot(
    const PortfolioSnapshot& snapshot,
    unsigned day,
    double cash,
    backtester::portfolio::PositionQuantity shares,
    double market_value,
    double equity
) {
    REQUIRE(snapshot.session_date() == session_date(day));
    require_near(snapshot.cash(), cash);
    REQUIRE(snapshot.position_quantity() == shares);
    require_near(snapshot.market_value(), market_value);
    require_near(snapshot.equity(), equity);
}

void require_equal_snapshots(
    std::span<const PortfolioSnapshot> lhs,
    std::span<const PortfolioSnapshot> rhs
) {
    REQUIRE(lhs.size() == rhs.size());
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        REQUIRE(lhs[i].session_date() == rhs[i].session_date());
        REQUIRE(lhs[i].cash() == rhs[i].cash());
        REQUIRE(lhs[i].position_quantity() == rhs[i].position_quantity());
        REQUIRE(lhs[i].market_value() == rhs[i].market_value());
        REQUIRE(lhs[i].equity() == rhs[i].equity());
    }
}

}  // namespace

TEST_CASE("Benchmark Oracle C executes Buy and Hold at the next available open") {
    auto engine = make_engine(oracle_c_bars(), true);
    const auto trace = run_to_completion(engine);

    const std::vector<EventKind> expected_events{
        EventKind::Market,
        EventKind::Signal,
        EventKind::Order,
        EventKind::Fill,
        EventKind::Market,
        EventKind::Market,
        EventKind::Market
    };
    REQUIRE(trace.event_kinds == expected_events);

    const std::vector<SessionDate> expected_dates{
        session_date(14), session_date(15),
        session_date(16), session_date(17)
    };
    REQUIRE(trace.market_dates == expected_dates);
    REQUIRE(trace.signal_dates == std::vector<SessionDate>{session_date(14)});

    // Initial signal is at Close(14); the only fill is at Open(15).
    REQUIRE(trace.cash_at_fill.size() == 1);
    REQUIRE(trace.shares_at_fill.size() == 1);
    require_near(trace.cash_at_fill.front(), 0.0);
    REQUIRE(trace.shares_at_fill.front() == 10);

    const auto& snapshots = engine.portfolio().snapshots();
    REQUIRE(snapshots.size() == 4);
    require_snapshot(snapshots[0], 14, 1'000.0,  0,    0.0, 1'000.0);
    require_snapshot(snapshots[1], 15,     0.0, 10, 1'100.0, 1'100.0);
    require_snapshot(snapshots[2], 16,     0.0, 10, 1'200.0, 1'200.0);
    require_snapshot(snapshots[3], 17,     0.0, 10,   900.0,   900.0);

    REQUIRE(engine.portfolio().position_quantity() == 10);
    require_near(engine.portfolio().cash(), 0.0);
}

TEST_CASE("Benchmark Oracle C no-trade strategy remains in cash") {
    auto engine = make_engine(oracle_c_bars(), false);
    const auto trace = run_to_completion(engine);

    const std::vector<EventKind> expected_events{
        EventKind::Market, EventKind::Market,
        EventKind::Market, EventKind::Market
    };
    REQUIRE(trace.event_kinds == expected_events);
    REQUIRE(trace.signal_dates.empty());
    REQUIRE(trace.cash_at_fill.empty());

    const auto& snapshots = engine.portfolio().snapshots();
    REQUIRE(snapshots.size() == 4);
    for (std::size_t i = 0; i < snapshots.size(); ++i) {
        require_snapshot(snapshots[i], 14u + static_cast<unsigned>(i),
                         1'000.0, 0, 0.0, 1'000.0);
    }
}

TEST_CASE("Benchmark Oracle C has aligned runs and independent absolute metrics") {
    const auto bars = oracle_c_bars();
    auto benchmark = make_engine(bars, true);
    auto strategy = make_engine(bars, false);

    const auto benchmark_trace = run_to_completion(benchmark);
    const auto strategy_trace = run_to_completion(strategy);
    REQUIRE(benchmark_trace.market_dates == strategy_trace.market_dates);

    const auto& benchmark_snapshots = benchmark.portfolio().snapshots();
    const auto& strategy_snapshots = strategy.portfolio().snapshots();
    REQUIRE(benchmark_snapshots.size() == strategy_snapshots.size());
    REQUIRE(benchmark_snapshots.size() == bars.size());

    for (std::size_t i = 0; i < bars.size(); ++i) {
        REQUIRE(benchmark_snapshots[i].session_date() == bars[i].session_date());
        REQUIRE(strategy_snapshots[i].session_date() == bars[i].session_date());
    }

    const auto benchmark_returns = build_return_series(
        initial_capital, benchmark_snapshots
    );
    const auto strategy_returns = build_return_series(
        initial_capital, strategy_snapshots
    );
    REQUIRE(benchmark_returns.size() == 4);
    REQUIRE(strategy_returns.size() == 4);

    const std::array<double, 4> expected_benchmark_returns{
        0.0, 0.10, 1.0 / 11.0, -0.25
    };
    for (std::size_t i = 0; i < 4; ++i) {
        REQUIRE(benchmark_returns[i].session_date == strategy_returns[i].session_date);
        REQUIRE(benchmark_returns[i].simple_return.has_value());
        REQUIRE(strategy_returns[i].simple_return.has_value());
        require_near(*benchmark_returns[i].simple_return,
                     expected_benchmark_returns[i]);
        require_near(*strategy_returns[i].simple_return, 0.0);
    }

    const auto benchmark_metrics = calculate_performance_metrics(
        initial_capital, benchmark_snapshots, benchmark_returns
    );
    const auto strategy_metrics = calculate_performance_metrics(
        initial_capital, strategy_snapshots, strategy_returns
    );

    REQUIRE(benchmark_metrics.total_return.has_value());
    REQUIRE(benchmark_metrics.maximum_drawdown.has_value());
    REQUIRE(strategy_metrics.total_return.has_value());
    REQUIRE(strategy_metrics.maximum_drawdown.has_value());
    require_near(*benchmark_metrics.total_return, -0.10);
    require_near(*benchmark_metrics.maximum_drawdown, 0.25);
    require_near(*strategy_metrics.total_return, 0.0);
    require_near(*strategy_metrics.maximum_drawdown, 0.0);
    require_near(
        *strategy_metrics.total_return - *benchmark_metrics.total_return,
        0.10
    );
    REQUIRE(strategy_metrics.annualized_volatility.has_value());
    require_near(*strategy_metrics.annualized_volatility, 0.0);
    REQUIRE_FALSE(strategy_metrics.sharpe_ratio.has_value());
}

TEST_CASE("Benchmark one-bar run never fabricates a terminal fill") {
    const auto bars = oracle_c_bars();
    auto engine = make_engine({bars.front()}, true);
    const auto trace = run_to_completion(engine);

    const std::vector<EventKind> expected_events{
        EventKind::Market, EventKind::Signal, EventKind::Order
    };
    REQUIRE(trace.event_kinds == expected_events);
    REQUIRE(trace.cash_at_fill.empty());

    const auto& snapshots = engine.portfolio().snapshots();
    REQUIRE(snapshots.size() == 1);
    require_snapshot(snapshots[0], 14, 1'000.0, 0, 0.0, 1'000.0);
    REQUIRE(engine.portfolio().position_quantity() == 0);
}

TEST_CASE("Benchmark with unaffordable whole shares remains uninvested") {
    auto engine = make_engine(oracle_c_bars(), true, 50.0);
    const auto trace = run_to_completion(engine);

    REQUIRE(count_events(trace, EventKind::Market) == 4);
    REQUIRE(count_events(trace, EventKind::Fill) == 0);
    REQUIRE(trace.signal_dates.size() == 1);
    REQUIRE(engine.portfolio().position_quantity() == 0);
    require_near(engine.portfolio().cash(), 50.0);

    const auto& snapshots = engine.portfolio().snapshots();
    REQUIRE(snapshots.size() == 4);
    for (std::size_t i = 0; i < snapshots.size(); ++i) {
        require_snapshot(snapshots[i], 14u + static_cast<unsigned>(i),
                         50.0, 0, 0.0, 50.0);
    }
}

TEST_CASE("Benchmark next-open gap clips the requested whole-share quantity") {
    const std::vector<Bar> bars{
        Bar{session_date(14), "AAPL", 100.0, 101.0, 99.0, 100.0, 1'000},
        Bar{session_date(15), "AAPL", 125.0, 135.0, 124.0, 130.0, 1'100},
        Bar{session_date(16), "AAPL", 130.0, 135.0, 119.0, 120.0, 1'200}
    };
    auto engine = make_engine(bars, true);
    const auto trace = run_to_completion(engine);

    // Close(14) implies a 10-share request. Open(15)=125 funds only 8.
    REQUIRE(count_events(trace, EventKind::Signal) == 1);
    REQUIRE(count_events(trace, EventKind::Order) == 1);
    REQUIRE(count_events(trace, EventKind::Fill) == 1);
    REQUIRE(trace.shares_at_fill.front() == 8);
    require_near(trace.cash_at_fill.front(), 0.0);

    const auto& snapshots = engine.portfolio().snapshots();
    REQUIRE(snapshots.size() == 3);
    require_snapshot(snapshots[0], 14, 1'000.0, 0,    0.0, 1'000.0);
    require_snapshot(snapshots[1], 15,     0.0, 8, 1'040.0, 1'040.0);
    require_snapshot(snapshots[2], 16,     0.0, 8,   960.0,   960.0);
}

TEST_CASE("Benchmark preserves residual cash after a partial affordable fill") {
    const std::vector<Bar> bars{
        Bar{session_date(14), "AAPL", 100.0, 101.0, 99.0, 100.0, 1'000},
        Bar{session_date(15), "AAPL", 105.0, 112.0, 104.0, 110.0, 1'100},
        Bar{session_date(16), "AAPL", 110.0, 112.0, 89.0, 90.0, 1'200}
    };
    auto engine = make_engine(bars, true);
    const auto trace = run_to_completion(engine);

    // Buy 9 shares at 105; 1000 - 9*105 = 55 cash remains.
    REQUIRE(count_events(trace, EventKind::Fill) == 1);
    REQUIRE(trace.shares_at_fill.front() == 9);
    require_near(trace.cash_at_fill.front(), 55.0);

    const auto& snapshots = engine.portfolio().snapshots();
    REQUIRE(snapshots.size() == 3);
    require_snapshot(snapshots[0], 14, 1'000.0, 0,   0.0, 1'000.0);
    require_snapshot(snapshots[1], 15,    55.0, 9, 990.0, 1'045.0);
    require_snapshot(snapshots[2], 16,    55.0, 9, 810.0,   865.0);
}

TEST_CASE("Benchmark runs are reproducible and never share mutable state") {
    const auto bars = oracle_c_bars();
    auto first = make_engine(bars, true);
    auto second = make_engine(bars, true);
    auto flat = make_engine(bars, false);

    const auto first_trace = run_to_completion(first);
    const auto flat_trace = run_to_completion(flat);
    const auto second_trace = run_to_completion(second);

    REQUIRE(first_trace.event_kinds == second_trace.event_kinds);
    REQUIRE(first_trace.market_dates == second_trace.market_dates);
    REQUIRE(first_trace.signal_dates == second_trace.signal_dates);
    REQUIRE(first_trace.cash_at_fill == second_trace.cash_at_fill);
    REQUIRE(first_trace.shares_at_fill == second_trace.shares_at_fill);
    require_equal_snapshots(first.portfolio().snapshots(), second.portfolio().snapshots());

    REQUIRE(flat_trace.cash_at_fill.empty());
    REQUIRE(flat.portfolio().position_quantity() == 0);
    REQUIRE(first.portfolio().position_quantity() == 10);
    REQUIRE(second.portfolio().position_quantity() == 10);
    REQUIRE(bars.size() == 4);
    REQUIRE(bars.front().session_date() == session_date(14));
    REQUIRE(bars.back().session_date() == session_date(17));
}

TEST_CASE("Benchmark empty feed produces no events snapshots or positions") {
    auto engine = make_engine({}, true);
    const auto trace = run_to_completion(engine);
    REQUIRE(trace.event_kinds.empty());
    REQUIRE(engine.portfolio().snapshots().empty());
    REQUIRE(engine.portfolio().position_quantity() == 0);
    require_near(engine.portfolio().cash(), initial_capital);
}


TEST_CASE(
    "Benchmark Oracle C validates the complete relative performance pipeline"
) {
    constexpr double annualization_factor = 4.0;
    constexpr double risk_free_rate_annual = 0.0;

    const auto bars = oracle_c_bars();

    auto strategy = make_engine(bars, false);
    auto benchmark = make_engine(bars, true);

    const auto strategy_trace = run_to_completion(strategy);
    const auto benchmark_trace = run_to_completion(benchmark);

    REQUIRE(
        strategy_trace.market_dates ==
        benchmark_trace.market_dates
    );

    const auto& strategy_snapshots =
        strategy.portfolio().snapshots();

    const auto& benchmark_snapshots =
        benchmark.portfolio().snapshots();

    REQUIRE(strategy_snapshots.size() == bars.size());
    REQUIRE(benchmark_snapshots.size() == bars.size());

    for (std::size_t i = 0; i < bars.size(); ++i) {
        REQUIRE(
            strategy_snapshots[i].session_date() ==
            bars[i].session_date()
        );

        REQUIRE(
            benchmark_snapshots[i].session_date() ==
            bars[i].session_date()
        );
    }

    // Build both return series from actual engine snapshots.
    const auto strategy_returns = build_return_series(
        initial_capital,
        strategy_snapshots
    );

    const auto benchmark_returns = build_return_series(
        initial_capital,
        benchmark_snapshots
    );

    // Preserve the full common investment horizon.
    const auto active_returns =
        backtester::performance::build_active_return_series(
            strategy_returns,
            benchmark_returns
        );

    const std::array<double, 4> expected_active{
        0.0,
        -0.10,
        -1.0 / 11.0,
        0.25
    };

    REQUIRE(active_returns.size() == expected_active.size());

    for (std::size_t i = 0; i < expected_active.size(); ++i) {
        REQUIRE(
            active_returns[i].session_date ==
            bars[i].session_date()
        );

        REQUIRE(active_returns[i].active_return.has_value());

        require_near(
            *active_returns[i].active_return,
            expected_active[i]
        );
    }

    // Calculate absolute performance independently for each run.
    const auto strategy_metrics = calculate_performance_metrics(
        initial_capital,
        strategy_snapshots,
        strategy_returns,
        annualization_factor,
        risk_free_rate_annual
    );

    const auto benchmark_metrics = calculate_performance_metrics(
        initial_capital,
        benchmark_snapshots,
        benchmark_returns,
        annualization_factor,
        risk_free_rate_annual
    );

    REQUIRE(strategy_metrics.total_return.has_value());
    REQUIRE(benchmark_metrics.total_return.has_value());
    REQUIRE(strategy_metrics.cagr.has_value());
    REQUIRE(benchmark_metrics.cagr.has_value());

    require_near(*strategy_metrics.total_return, 0.0);
    require_near(*benchmark_metrics.total_return, -0.10);

    // A = 4 and N = 4, so CAGR equals total return
    // for this particular deterministic oracle.
    require_near(*strategy_metrics.cagr, 0.0);
    require_near(*benchmark_metrics.cagr, -0.10);

    // Calculate all seven relative metrics from actual runs.
    const auto relative =
        backtester::performance::calculate_relative_performance_metrics(
            strategy_metrics,
            benchmark_metrics,
            strategy_returns,
            benchmark_returns,
            annualization_factor,
            risk_free_rate_annual
        );

    REQUIRE(relative.excess_total_return.has_value());
    REQUIRE(relative.annualized_return_difference.has_value());
    REQUIRE(relative.tracking_error.has_value());
    REQUIRE(relative.information_ratio.has_value());
    REQUIRE(relative.beta.has_value());
    REQUIRE(relative.jensen_alpha.has_value());

    // Independent Oracle C reference values.
    require_near(*relative.excess_total_return, 0.10);
    require_near(*relative.annualized_return_difference, 0.10);

    require_near(
        *relative.tracking_error,
        0.32637714161303194
    );

    require_near(
        *relative.information_ratio,
        0.18105100375249333
    );

    // The strategy is constant, so correlation is undefined.
    REQUIRE_FALSE(relative.correlation.has_value());

    // Benchmark variance is positive, so beta is defined.
    require_near(*relative.beta, 0.0);
    require_near(*relative.jensen_alpha, 0.0);

    // Both portfolios retain their actual terminal holdings.
    REQUIRE(strategy.portfolio().position_quantity() == 0);
    REQUIRE(benchmark.portfolio().position_quantity() == 10);
}

TEST_CASE(
    "Benchmark comparison handles a benchmark that cannot buy whole shares"
) {
    constexpr double capital = 50.0;
    constexpr double annualization_factor = 4.0;

    const auto bars = oracle_c_bars();

    auto strategy = make_engine(bars, false, capital);
    auto benchmark = make_engine(bars, true, capital);

    const auto strategy_trace = run_to_completion(strategy);
    const auto benchmark_trace = run_to_completion(benchmark);

    REQUIRE(strategy_trace.market_dates == benchmark_trace.market_dates);
    REQUIRE(benchmark_trace.cash_at_fill.empty());

    REQUIRE(strategy.portfolio().position_quantity() == 0);
    REQUIRE(benchmark.portfolio().position_quantity() == 0);

    const auto strategy_returns = build_return_series(
        capital,
        strategy.portfolio().snapshots()
    );

    const auto benchmark_returns = build_return_series(
        capital,
        benchmark.portfolio().snapshots()
    );

    const auto strategy_metrics = calculate_performance_metrics(
        capital,
        strategy.portfolio().snapshots(),
        strategy_returns,
        annualization_factor,
        0.0
    );

    const auto benchmark_metrics = calculate_performance_metrics(
        capital,
        benchmark.portfolio().snapshots(),
        benchmark_returns,
        annualization_factor,
        0.0
    );

    const auto relative =
        backtester::performance::calculate_relative_performance_metrics(
            strategy_metrics,
            benchmark_metrics,
            strategy_returns,
            benchmark_returns,
            annualization_factor,
            0.0
        );

    REQUIRE(relative.excess_total_return.has_value());
    REQUIRE(relative.annualized_return_difference.has_value());
    REQUIRE(relative.tracking_error.has_value());

    require_near(*relative.excess_total_return, 0.0);
    require_near(*relative.annualized_return_difference, 0.0);
    require_near(*relative.tracking_error, 0.0);

    // Both histories are constant: regression is undefined.
    REQUIRE_FALSE(relative.information_ratio.has_value());
    REQUIRE_FALSE(relative.correlation.has_value());
    REQUIRE_FALSE(relative.beta.has_value());
    REQUIRE_FALSE(relative.jensen_alpha.has_value());
}
