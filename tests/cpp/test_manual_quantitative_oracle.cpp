#include "backtester/data/historical_data_feed.hpp"
#include "backtester/engine/backtest_engine.hpp"
#include "backtester/events/event.hpp"
#include "backtester/market/bar.hpp"
#include "backtester/performance/active_return_series.hpp"
#include "backtester/performance/performance_metrics.hpp"
#include "backtester/performance/relative_metrics.hpp"
#include "backtester/performance/return_series.hpp"
#include "backtester/portfolio/portfolio.hpp"
#include "backtester/portfolio/position_sizer.hpp"
#include "backtester/strategy/buy_and_hold_strategy.hpp"
#include "backtester/strategy/price_filter_strategy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <variant>

namespace {

using backtester::data::HistoricalDataFeed;
using backtester::engine::BacktestEngine;
using backtester::events::Event;
using backtester::events::FillEvent;
using backtester::events::MarketEvent;
using backtester::events::OrderEvent;
using backtester::events::OrderSide;
using backtester::events::OrderType;
using backtester::events::SignalDirection;
using backtester::events::SignalEvent;
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
using backtester::strategy::BuyAndHoldStrategy;
using backtester::strategy::PriceFilterStrategy;

constexpr double capital = 1'000.0;
constexpr double annualization = 4.0;
constexpr double risk_free_annual = 0.04;

// Approved manual oracle: fixed, synthetic daily OHLC observations.
// Date: September 2026. None of these values is calculated by production code.
struct ManualBar final {
    unsigned day;
    double open;
    double high;
    double low;
    double close;
};

constexpr std::array<ManualBar, 9> manual_bars{{
    {14, 100, 100, 100, 100},
    {15, 105, 110, 105, 110},
    {16, 120, 125, 120, 125},
    {17, 130, 130, 130, 130},
    {18, 128, 128, 119, 119},
    {21, 112, 114, 112, 114},
    {22, 118, 122, 118, 122},
    {23, 128, 128, 127, 127},
    {24, 123, 123, 118, 118}
}};

SessionDate date(unsigned day) {
    return SessionDate{
        std::chrono::year{2026},
        std::chrono::month{9},
        std::chrono::day{day}
    };
}

std::vector<Bar> make_bars() {
    std::vector<Bar> bars;
    bars.reserve(manual_bars.size());

    for (std::size_t i = 0; i < manual_bars.size(); ++i) {
        const auto& row = manual_bars[i];
        bars.emplace_back(
            date(row.day), "AAPL",
            row.open, row.high, row.low, row.close,
            static_cast<std::uint64_t>(1'000 + i)
        );
    }
    return bars;
}

BacktestEngine make_strategy_engine() {
    return BacktestEngine{
        HistoricalDataFeed{make_bars()},
        std::make_unique<PriceFilterStrategy>("AAPL", 1, 0.05),
        Portfolio{"AAPL", capital},
        PositionSizer{1.0}
    };
}

BacktestEngine make_benchmark_engine() {
    return BacktestEngine{
        HistoricalDataFeed{make_bars()},
        std::make_unique<BuyAndHoldStrategy>("AAPL"),
        Portfolio{"AAPL", capital},
        PositionSizer{1.0}
    };
}

enum class Kind { Market, Signal, Order, Fill };

struct RecordedSignal final {
    SessionDate day;
    SignalDirection direction;
    bool operator==(const RecordedSignal&) const = default;
};

struct RecordedOrder final {
    SessionDate day;
    OrderSide side;
    std::uint64_t quantity;
    OrderType type;
    bool operator==(const RecordedOrder&) const = default;
};

struct RecordedFill final {
    SessionDate day;
    OrderSide side;
    std::uint64_t quantity;
    double price;
    double fees;
    double cash_after;
    PositionQuantity shares_after;
    std::size_t snapshots_before_fill;
    bool operator==(const RecordedFill&) const = default;
};

struct Trace final {
    std::vector<Kind> kinds;
    std::vector<SessionDate> market_dates;
    std::vector<RecordedSignal> signals;
    std::vector<RecordedOrder> orders;
    std::vector<RecordedFill> fills;
};

Trace execute(BacktestEngine& engine) {
    Trace trace;

    while (const auto event = engine.step()) {
        const Event& value = *event;

        if (std::holds_alternative<MarketEvent>(value)) {
            trace.kinds.push_back(Kind::Market);
            const auto& bar = std::get<MarketEvent>(value).bar();
            REQUIRE(bar.symbol() == "AAPL");
            const std::size_t bar_index = trace.market_dates.size();
            REQUIRE(bar_index < manual_bars.size());
            const auto& expected_bar = manual_bars[bar_index];
            REQUIRE(bar.session_date() == date(expected_bar.day));
            REQUIRE(bar.open() == expected_bar.open);
            REQUIRE(bar.high() == expected_bar.high);
            REQUIRE(bar.low() == expected_bar.low);
            REQUIRE(bar.close() == expected_bar.close);
            REQUIRE(bar.volume() == static_cast<std::uint64_t>(1'000 + bar_index));
            trace.market_dates.push_back(bar.session_date());
        } else if (std::holds_alternative<SignalEvent>(value)) {
            trace.kinds.push_back(Kind::Signal);
            const auto& signal = std::get<SignalEvent>(value);
            REQUIRE(signal.symbol() == "AAPL");
            REQUIRE(signal.direction() != SignalDirection::Short);
            trace.signals.push_back({signal.signal_date(), signal.direction()});
        } else if (std::holds_alternative<OrderEvent>(value)) {
            trace.kinds.push_back(Kind::Order);
            const auto& order = std::get<OrderEvent>(value);
            REQUIRE(order.symbol() == "AAPL");
            trace.orders.push_back({
                order.submission_date(), order.side(),
                order.quantity(), order.order_type()
            });
        } else if (std::holds_alternative<FillEvent>(value)) {
            trace.kinds.push_back(Kind::Fill);
            const auto& fill = std::get<FillEvent>(value);
            REQUIRE(fill.symbol() == "AAPL");
            trace.fills.push_back({
                fill.fill_date(), fill.side(), fill.quantity(),
                fill.fill_price(), fill.fees(),
                engine.portfolio().cash(),
                engine.portfolio().position_quantity(),
                engine.portfolio().snapshots().size()
            });
        } else {
            FAIL("Unrecognized event type");
        }
    }

    REQUIRE(engine.finished());
    REQUIRE_FALSE(engine.step().has_value());
    return trace;
}

void require_near(double actual, double expected, double tolerance = 1e-10) {
    REQUIRE(std::isfinite(actual));
    REQUIRE(std::abs(actual - expected) <= tolerance);
}

struct ExpectedPortfolioRow final {
    double cash;
    PositionQuantity shares;
    double equity;
};

// Hand-calculated from the transaction ledger, not from the backtester.
constexpr std::array<ExpectedPortfolioRow, 9> strategy_rows{{
    {1000, 0, 1000},
    {1000, 0, 1000},
    {  40, 8, 1040},
    {  40, 8, 1080},
    {  40, 8,  992},
    { 936, 0,  936},
    { 936, 0,  936},
    {  40, 7,  929},
    {  40, 7,  866}
}};

constexpr std::array<ExpectedPortfolioRow, 9> benchmark_rows{{
    {1000, 0, 1000},
    {  55, 9, 1045},
    {  55, 9, 1180},
    {  55, 9, 1225},
    {  55, 9, 1126},
    {  55, 9, 1081},
    {  55, 9, 1153},
    {  55, 9, 1198},
    {  55, 9, 1117}
}};

void require_snapshots(
    const BacktestEngine& engine,
    const std::array<ExpectedPortfolioRow, 9>& expected
) {
    const auto& actual = engine.portfolio().snapshots();
    REQUIRE(actual.size() == expected.size());

    for (std::size_t i = 0; i < expected.size(); ++i) {
        const PortfolioSnapshot& snapshot = actual[i];
        REQUIRE(snapshot.session_date() == date(manual_bars[i].day));
        require_near(snapshot.cash(), expected[i].cash);
        REQUIRE(snapshot.position_quantity() == expected[i].shares);
        require_near(
            snapshot.market_value(),
            static_cast<double>(expected[i].shares) * manual_bars[i].close
        );
        require_near(snapshot.equity(), expected[i].equity);
    }

    require_near(engine.portfolio().cash(), expected.back().cash);
    REQUIRE(engine.portfolio().position_quantity() == expected.back().shares);
    require_near(engine.portfolio().latest_equity(), expected.back().equity);
}

// The first return is measured against the initial $1,000 anchor.
// These expressions contain only fixed manually specified equity values.
constexpr std::array<double, 9> strategy_returns{{
    0.0, 0.0, 40.0 / 1000.0, 40.0 / 1040.0,
    -88.0 / 1080.0, -56.0 / 992.0, 0.0,
    -7.0 / 936.0, -63.0 / 929.0
}};

constexpr std::array<double, 9> benchmark_returns{{
    0.0, 45.0 / 1000.0, 135.0 / 1045.0, 45.0 / 1180.0,
    -99.0 / 1225.0, -45.0 / 1126.0,
    72.0 / 1081.0, 45.0 / 1153.0, -81.0 / 1198.0
}};

void require_optional(
    const std::optional<double>& actual,
    double expected
) {
    REQUIRE(actual.has_value());
    require_near(*actual, expected);
}

}  // namespace

TEST_CASE("ManualOracle executes the complete strategy and benchmark lifecycle") {
    auto strategy = make_strategy_engine();
    auto benchmark = make_benchmark_engine();

    const auto strategy_trace = execute(strategy);
    const auto benchmark_trace = execute(benchmark);

    const std::vector<SessionDate> dates{
        date(14), date(15), date(16), date(17), date(18),
        date(21), date(22), date(23), date(24)
    };
    REQUIRE(strategy_trace.market_dates == dates);
    REQUIRE(benchmark_trace.market_dates == dates);

    // Strategy: N=1, strictly outside +/-5% of the previous Close.
    REQUIRE(strategy_trace.signals == (std::vector<RecordedSignal>{
        {date(15), SignalDirection::Long},
        {date(18), SignalDirection::Flat},
        {date(22), SignalDirection::Long},
        {date(24), SignalDirection::Flat}
    }));

    REQUIRE(strategy_trace.orders == (std::vector<RecordedOrder>{
        {date(15), OrderSide::Buy,  9, OrderType::Market},
        {date(18), OrderSide::Sell, 8, OrderType::Market},
        {date(22), OrderSide::Buy,  7, OrderType::Market},
        {date(24), OrderSide::Sell, 7, OrderType::Market}
    }));

    // The first order requests 9 shares but the next Open=120 funds only 8.
    // The final Sell is never filled because no later Open exists.
    REQUIRE(strategy_trace.fills == (std::vector<RecordedFill>{
        {date(16), OrderSide::Buy,  8, 120, 0,  40, 8, 2},
        {date(21), OrderSide::Sell, 8, 112, 0, 936, 0, 5},
        {date(23), OrderSide::Buy,  7, 128, 0,  40, 7, 7}
    }));

    const std::vector<Kind> strategy_event_order{
        Kind::Market,                         // Sep 14
        Kind::Market, Kind::Signal, Kind::Order, // Sep 15
        Kind::Fill, Kind::Market,             // Sep 16
        Kind::Market,                         // Sep 17
        Kind::Market, Kind::Signal, Kind::Order, // Sep 18
        Kind::Fill, Kind::Market,             // Sep 21
        Kind::Market, Kind::Signal, Kind::Order, // Sep 22
        Kind::Fill, Kind::Market,             // Sep 23
        Kind::Market, Kind::Signal, Kind::Order  // Sep 24
    };
    REQUIRE(strategy_trace.kinds == strategy_event_order);

    REQUIRE(benchmark_trace.signals == (std::vector<RecordedSignal>{
        {date(14), SignalDirection::Long}
    }));
    REQUIRE(benchmark_trace.orders == (std::vector<RecordedOrder>{
        {date(14), OrderSide::Buy, 10, OrderType::Market}
    }));
    REQUIRE(benchmark_trace.fills == (std::vector<RecordedFill>{
        {date(15), OrderSide::Buy, 9, 105, 0, 55, 9, 1}
    }));
    const std::vector<Kind> benchmark_event_order{
        Kind::Market, Kind::Signal, Kind::Order,
        Kind::Fill, Kind::Market,
        Kind::Market, Kind::Market, Kind::Market,
        Kind::Market, Kind::Market, Kind::Market, Kind::Market
    };
    REQUIRE(benchmark_trace.kinds == benchmark_event_order);

    require_snapshots(strategy, strategy_rows);
    require_snapshots(benchmark, benchmark_rows);
}

TEST_CASE("ManualOracle validates returns absolute metrics and relative metrics") {
    auto strategy = make_strategy_engine();
    auto benchmark = make_benchmark_engine();

    const auto strategy_trace = execute(strategy);
    const auto benchmark_trace = execute(benchmark);
    REQUIRE(strategy_trace.market_dates == benchmark_trace.market_dates);

    // Independent daily equity tables must match before calculating returns.
    require_snapshots(strategy, strategy_rows);
    require_snapshots(benchmark, benchmark_rows);

    const auto sr = build_return_series(capital, strategy.portfolio().snapshots());
    const auto br = build_return_series(capital, benchmark.portfolio().snapshots());
    const auto active = build_active_return_series(sr, br);

    REQUIRE(sr.size() == manual_bars.size());
    REQUIRE(br.size() == manual_bars.size());
    REQUIRE(active.size() == manual_bars.size());

    for (std::size_t i = 0; i < manual_bars.size(); ++i) {
        const auto expected_date = date(manual_bars[i].day);
        REQUIRE(sr[i].session_date == expected_date);
        REQUIRE(br[i].session_date == expected_date);
        REQUIRE(active[i].session_date == expected_date);

        require_optional(sr[i].simple_return, strategy_returns[i]);
        require_optional(br[i].simple_return, benchmark_returns[i]);
        require_optional(
            active[i].active_return,
            strategy_returns[i] - benchmark_returns[i]
        );
        require_optional(sr[i].log_return, std::log1p(strategy_returns[i]));
        require_optional(br[i].log_return, std::log1p(benchmark_returns[i]));
    }

    const auto sm = calculate_performance_metrics(
        capital, strategy.portfolio().snapshots(), sr,
        annualization, risk_free_annual
    );
    const auto bm = calculate_performance_metrics(
        capital, benchmark.portfolio().snapshots(), br,
        annualization, risk_free_annual
    );

    // External arithmetic oracle: sample volatility and effective periodic RF.
    require_optional(sm.total_return, -0.134);
    require_optional(sm.cagr, -0.06194095749275619);
    require_optional(sm.annualized_volatility, 0.08815365391628664);
    require_optional(sm.sharpe_ratio, -1.1265466204531462);
    require_optional(sm.maximum_drawdown, 214.0 / 1080.0);

    require_optional(bm.total_return, 0.117);
    require_optional(bm.cagr, 0.050405448589513124);
    require_optional(bm.annualized_volatility, 0.135946163965103);
    require_optional(bm.sharpe_ratio, 0.13365311338115607);
    require_optional(bm.maximum_drawdown, 144.0 / 1225.0);

    const auto rm = calculate_relative_performance_metrics(
        sm, bm, sr, br, annualization, risk_free_annual
    );

    require_optional(rm.excess_total_return, -0.251);
    require_optional(rm.annualized_return_difference, -0.11234640608226931);
    require_optional(rm.tracking_error, 0.06740770749663236);
    require_optional(rm.information_ratio, -1.7428100335858374);
    require_optional(rm.correlation, 0.9057223579574597);
    require_optional(rm.beta, 0.5873114250441107);
    require_optional(rm.jensen_alpha, -0.10998043105204756);

    // Performance is read-only; neither engine may change its final holdings.
    REQUIRE(strategy.portfolio().position_quantity() == 7);
    REQUIRE(benchmark.portfolio().position_quantity() == 9);
    require_near(strategy.portfolio().cash(), 40.0);
    require_near(benchmark.portfolio().cash(), 55.0);
}
