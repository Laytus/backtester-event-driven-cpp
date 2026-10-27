#include "backtester/performance/performance_metrics.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

using backtester::market::SessionDate;
using backtester::performance::PerformanceMetrics;
using backtester::performance::ReturnObservation;
using backtester::performance::ReturnSeries;
using backtester::performance::build_return_series;
using backtester::performance::calculate_annualized_volatility;
using backtester::performance::calculate_cagr;
using backtester::performance::calculate_maximum_drawdown;
using backtester::performance::calculate_performance_metrics;
using backtester::performance::calculate_sharpe_ratio;
using backtester::performance::calculate_total_return;
using backtester::portfolio::PortfolioSnapshot;

SessionDate session_date(int offset) {
    const std::chrono::sys_days first{
        std::chrono::year{2026} / std::chrono::September / 14
    };
    return SessionDate{first + std::chrono::days{offset}};
}

PortfolioSnapshot make_snapshot(int offset, double equity) {
    // Synthetic analytics fixture: cash + market value == equity.
    return PortfolioSnapshot{session_date(offset), equity, 0, 0.0, equity};
}

std::vector<PortfolioSnapshot> make_snapshots(
    std::initializer_list<double> equities
) {
    std::vector<PortfolioSnapshot> snapshots;
    snapshots.reserve(equities.size());
    int offset = 0;
    for (const double equity : equities) {
        snapshots.push_back(make_snapshot(offset++, equity));
    }
    return snapshots;
}

ReturnSeries make_returns(
    std::initializer_list<std::optional<double>> simple_returns
) {
    ReturnSeries observations;
    observations.reserve(simple_returns.size());
    int offset = 0;
    for (const auto& value : simple_returns) {
        observations.push_back({session_date(offset++), value, std::nullopt});
    }
    return observations;
}

void require_close(
    double actual,
    double expected,
    double absolute_tolerance = 1e-12,
    double relative_tolerance = 1e-10
) {
    REQUIRE(std::isfinite(actual));
    REQUIRE(std::isfinite(expected));
    const double scale = std::max(std::abs(actual), std::abs(expected));
    const double tolerance = std::max(
        absolute_tolerance, relative_tolerance * scale
    );
    REQUIRE(std::abs(actual - expected) <= tolerance);
}

void require_present(
    const std::optional<double>& actual,
    double expected,
    double absolute_tolerance = 1e-12,
    double relative_tolerance = 1e-10
) {
    REQUIRE(actual.has_value());
    require_close(*actual, expected, absolute_tolerance, relative_tolerance);
}

void require_absent(const std::optional<double>& actual) {
    REQUIRE_FALSE(actual.has_value());
}

void require_equivalent(
    const std::optional<double>& actual,
    const std::optional<double>& expected
) {
    REQUIRE(actual.has_value() == expected.has_value());
    if (expected) {
        require_close(*actual, *expected);
    }
}

void require_matches_individual_metrics(
    double initial_equity,
    const std::vector<PortfolioSnapshot>& snapshots,
    const ReturnSeries& returns,
    double annualization = 252.0,
    double risk_free_rate = 0.0
) {
    const PerformanceMetrics aggregate = calculate_performance_metrics(
        initial_equity, snapshots, returns, annualization, risk_free_rate
    );
    require_equivalent(
        aggregate.total_return, calculate_total_return(initial_equity, snapshots)
    );
    require_equivalent(
        aggregate.cagr, calculate_cagr(initial_equity, snapshots, annualization)
    );
    require_equivalent(
        aggregate.annualized_volatility,
        calculate_annualized_volatility(returns, annualization)
    );
    require_equivalent(
        aggregate.sharpe_ratio,
        calculate_sharpe_ratio(returns, annualization, risk_free_rate)
    );
    require_equivalent(
        aggregate.maximum_drawdown,
        calculate_maximum_drawdown(initial_equity, snapshots)
    );
}

}  // namespace

// -----------------------------------------------------------------------------
// Total return
// -----------------------------------------------------------------------------

TEST_CASE("Performance total return requires a positive finite initial equity") {
    const auto snapshots = make_snapshots({1050.0});
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double invalid : {0.0, -0.0, -1.0, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            calculate_total_return(invalid, snapshots), std::invalid_argument
        );
    }
}

TEST_CASE("Performance total return validates the anchor for empty histories") {
    const std::vector<PortfolioSnapshot> snapshots;
    REQUIRE_THROWS_AS(calculate_total_return(0.0, snapshots), std::invalid_argument);
    require_absent(calculate_total_return(1000.0, snapshots));
}

TEST_CASE("Performance total return measures the initial-to-final equity change") {
    require_present(calculate_total_return(1000.0, make_snapshots({1131.9})), 0.1319);
    require_present(calculate_total_return(1000.0, make_snapshots({800.0})), -0.20);
    require_present(calculate_total_return(1000.0, make_snapshots({1000.0})), 0.0);
}

TEST_CASE("Performance total return supports zero and negative terminal equity") {
    require_present(calculate_total_return(1000.0, make_snapshots({0.0})), -1.0);
    require_present(calculate_total_return(1000.0, make_snapshots({-200.0})), -1.2);
}

TEST_CASE("Performance endpoint total return survives intermediate insolvency") {
    const auto snapshots = make_snapshots({800.0, 0.0, -100.0, 50.0});
    require_present(calculate_total_return(1000.0, snapshots), -0.95);
}

TEST_CASE("Performance total return rejects duplicate and decreasing snapshot dates") {
    auto duplicates = make_snapshots({1000.0, 1050.0});
    duplicates[1] = make_snapshot(0, 1050.0);
    REQUIRE_THROWS_AS(
        calculate_total_return(1000.0, duplicates), std::invalid_argument
    );

    auto reversed = make_snapshots({1000.0, 1050.0});
    std::reverse(reversed.begin(), reversed.end());
    REQUIRE_THROWS_AS(
        calculate_total_return(1000.0, reversed), std::invalid_argument
    );
}

TEST_CASE("Performance total return rejects nonfinite arithmetic overflow") {
    const double tiny = std::numeric_limits<double>::denorm_min();
    const double maximum = std::numeric_limits<double>::max();
    REQUIRE_THROWS_AS(
        calculate_total_return(tiny, make_snapshots({maximum})),
        std::overflow_error
    );
    REQUIRE_THROWS_AS(
        calculate_total_return(tiny, make_snapshots({-maximum})),
        std::overflow_error
    );
}

// -----------------------------------------------------------------------------
// CAGR
// -----------------------------------------------------------------------------

TEST_CASE("Performance CAGR is unavailable for an empty history") {
    const std::vector<PortfolioSnapshot> snapshots;
    require_absent(calculate_cagr(1000.0, snapshots));
}

TEST_CASE("Performance CAGR includes the initial-to-first-snapshot observation") {
    // With A=1 and exactly one return observation, CAGR equals total return.
    require_present(calculate_cagr(100.0, make_snapshots({110.0}), 1.0), 0.10);
}

TEST_CASE("Performance CAGR uses the observation count rather than calendar days") {
    std::vector<PortfolioSnapshot> snapshots;
    snapshots.push_back(make_snapshot(0, 110.0));
    snapshots.push_back(make_snapshot(30, 121.0));
    require_present(calculate_cagr(100.0, snapshots, 2.0), 0.21);
    require_present(calculate_cagr(100.0, snapshots, 1.0), 0.10);
}

TEST_CASE("Performance CAGR Oracle A matches the independent annualization") {
    const auto snapshots = make_snapshots({1000.0, 1050.0, 1029.0, 1131.9, 1131.9});
    require_present(calculate_cagr(1000.0, snapshots), 514.1411225394515, 1e-10);
}

TEST_CASE("Performance CAGR is zero for unchanged positive equity") {
    require_present(calculate_cagr(1000.0, make_snapshots({1000.0, 1000.0})), 0.0);
}

TEST_CASE("Performance CAGR is minus one when equity first reaches zero at the end") {
    require_present(calculate_cagr(1000.0, make_snapshots({900.0, 0.0})), -1.0);
    require_present(calculate_cagr(1000.0, make_snapshots({0.0})), -1.0);
}

TEST_CASE("Performance CAGR is unavailable following earlier zero equity") {
    require_absent(calculate_cagr(1000.0, make_snapshots({0.0, 1200.0})));
    require_absent(calculate_cagr(1000.0, make_snapshots({0.0, 0.0})));
}

TEST_CASE("Performance CAGR is unavailable following negative equity or with negative endpoint") {
    require_absent(calculate_cagr(1000.0, make_snapshots({900.0, -10.0, 1200.0})));
    require_absent(calculate_cagr(1000.0, make_snapshots({900.0, -10.0})));
}

TEST_CASE("Performance CAGR rejects invalid annualization factors") {
    const auto snapshots = make_snapshots({110.0});
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double invalid : {0.0, -1.0, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(calculate_cagr(100.0, snapshots, invalid), std::invalid_argument);
    }
}

TEST_CASE("Performance CAGR validates initial equity even when unavailable") {
    const auto snapshots = make_snapshots({-10.0});
    REQUIRE_THROWS_AS(calculate_cagr(0.0, snapshots), std::invalid_argument);
}

TEST_CASE("Performance CAGR rejects annualization overflow") {
    const double maximum = std::numeric_limits<double>::max();
    REQUIRE_THROWS_AS(
        calculate_cagr(1.0, make_snapshots({maximum}), 252.0),
        std::overflow_error
    );
}

TEST_CASE("Performance CAGR retains representable small positive changes") {
    const double first = 100.0;
    const double next = std::nextafter(first, 101.0);
    const double expected = (next - first) / first;
    require_present(calculate_cagr(first, make_snapshots({next}), 1.0), expected,
                    1e-18, 1e-10);
}

// -----------------------------------------------------------------------------
// Annualized volatility
// -----------------------------------------------------------------------------

TEST_CASE("Performance annualized volatility requires at least two returns") {
    const ReturnSeries empty;
    const auto single = make_returns({0.05});
    require_absent(calculate_annualized_volatility(empty));
    require_absent(calculate_annualized_volatility(single));
}

TEST_CASE("Performance annualized volatility matches independent Oracle A") {
    const auto returns = make_returns({0.0, 0.05, -0.02, 0.10, 0.0});
    require_present(calculate_annualized_volatility(returns), 0.774441734412603);
}

TEST_CASE("Performance annualized volatility uses sample rather than population dispersion") {
    const auto returns = make_returns({0.02, 0.06});
    require_present(calculate_annualized_volatility(returns, 1.0), std::sqrt(0.0008));
    require_present(calculate_annualized_volatility(returns, 4.0),
                    2.0 * std::sqrt(0.0008));
}

TEST_CASE("Performance annualized volatility is zero for constant simple returns") {
    require_present(calculate_annualized_volatility(make_returns({0.05, 0.05, 0.05})), 0.0);
    require_present(calculate_annualized_volatility(make_returns({0.0, 0.0})), 0.0);
}

TEST_CASE("Performance annualized volatility does not discard unavailable observations") {
    require_absent(calculate_annualized_volatility(make_returns({0.1, std::nullopt, 0.2})));
    require_absent(calculate_annualized_volatility(make_returns({std::nullopt, 0.1})));
}

TEST_CASE("Performance volatility ignores log returns and requires simple returns") {
    auto returns = make_returns({std::nullopt, 0.1});
    returns[0].log_return = 0.1;
    require_absent(calculate_annualized_volatility(returns));

    returns[0].simple_return = 0.0;
    returns[1].log_return = std::log1p(0.1);
    require_present(calculate_annualized_volatility(returns, 1.0), std::sqrt(0.005));
}

TEST_CASE("Performance annualized volatility rejects invalid annualization factors") {
    const auto returns = make_returns({0.0, 0.1});
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double invalid : {0.0, -252.0, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            calculate_annualized_volatility(returns, invalid), std::invalid_argument
        );
    }
}

TEST_CASE("Performance annualized volatility rejects nonfinite simple returns") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double invalid : {nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            calculate_annualized_volatility(make_returns({0.1, invalid})),
            std::invalid_argument
        );
    }
}

TEST_CASE("Performance annualized volatility rejects duplicate and unordered return dates") {
    auto returns = make_returns({0.0, 0.1});
    returns[1].session_date = session_date(0);
    REQUIRE_THROWS_AS(calculate_annualized_volatility(returns), std::invalid_argument);
    returns[0].session_date = session_date(5);
    REQUIRE_THROWS_AS(calculate_annualized_volatility(returns), std::invalid_argument);
}

TEST_CASE("Performance annualized volatility supports missing calendar dates") {
    auto returns = make_returns({0.0, 0.1});
    returns[1].session_date = session_date(30);
    require_present(calculate_annualized_volatility(returns, 1.0), std::sqrt(0.005));
}

TEST_CASE("Performance annualized volatility accepts a valid return below minus one") {
    require_present(calculate_annualized_volatility(make_returns({-1.2, 0.0}), 1.0),
                    std::sqrt(0.72));
}

TEST_CASE("Performance annualized volatility reports arithmetic overflow") {
    const double maximum = std::numeric_limits<double>::max();
    REQUIRE_THROWS_AS(
        calculate_annualized_volatility(make_returns({-maximum, maximum})),
        std::overflow_error
    );
}

// -----------------------------------------------------------------------------
// Sharpe ratio
// -----------------------------------------------------------------------------

TEST_CASE("Performance Sharpe matches independent Oracle A at zero risk-free rate") {
    const auto returns = make_returns({0.0, 0.05, -0.02, 0.10, 0.0});
    require_present(calculate_sharpe_ratio(returns), 8.460287854927591);
}

TEST_CASE("Performance Sharpe converts the effective annual risk-free rate") {
    const auto returns = make_returns({0.0, 0.05, -0.02, 0.10, 0.0});
    require_present(calculate_sharpe_ratio(returns, 252.0, 0.04), 8.409640060935512);
}

TEST_CASE("Performance Sharpe uses annualized arithmetic excess mean") {
    const auto returns = make_returns({0.0, 0.2});
    // A=1, risk-free periodic=0.05, sample mean=0.1, sample SD=sqrt(0.02).
    require_present(calculate_sharpe_ratio(returns, 1.0, 0.05),
                    0.05 / std::sqrt(0.02));
}

TEST_CASE("Performance Sharpe supports negative annual risk-free rates above minus one") {
    const auto returns = make_returns({0.0, 0.2});
    require_present(calculate_sharpe_ratio(returns, 1.0, -0.5),
                    0.6 / std::sqrt(0.02));
}

TEST_CASE("Performance Sharpe is unavailable for zero sample dispersion") {
    require_absent(calculate_sharpe_ratio(make_returns({0.0, 0.0})));
    require_absent(calculate_sharpe_ratio(make_returns({0.05, 0.05, 0.05})));
}

TEST_CASE("Performance Sharpe is unavailable with too few or missing returns") {
    require_absent(calculate_sharpe_ratio(ReturnSeries{}));
    require_absent(calculate_sharpe_ratio(make_returns({0.1})));
    require_absent(calculate_sharpe_ratio(make_returns({0.1, std::nullopt, 0.2})));
}

TEST_CASE("Performance Sharpe rejects invalid risk-free and annualization inputs") {
    const auto returns = make_returns({0.0, 0.2});
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double invalid : {-1.0, -2.0, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            calculate_sharpe_ratio(returns, 252.0, invalid),
            std::invalid_argument
        );
    }
    for (const double invalid : {0.0, -1.0, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            calculate_sharpe_ratio(returns, invalid, 0.0),
            std::invalid_argument
        );
    }
}

TEST_CASE("Performance Sharpe validates inputs even when dispersion is zero") {
    const auto returns = make_returns({0.0, 0.0});
    REQUIRE_THROWS_AS(
        calculate_sharpe_ratio(returns, 252.0, -1.0), std::invalid_argument
    );
}

TEST_CASE("Performance Sharpe rejects nonfinite input simple returns") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    REQUIRE_THROWS_AS(
        calculate_sharpe_ratio(make_returns({0.0, nan})), std::invalid_argument
    );
}

TEST_CASE("Performance Sharpe rejects overflow in periodic risk-free conversion") {
    const auto returns = make_returns({0.0, 0.2});
    const double maximum = std::numeric_limits<double>::max();
    REQUIRE_THROWS_AS(
        calculate_sharpe_ratio(returns, 0.001, maximum), std::overflow_error
    );
}

// -----------------------------------------------------------------------------
// Maximum drawdown
// -----------------------------------------------------------------------------

TEST_CASE("Performance maximum drawdown is unavailable without snapshots") {
    const std::vector<PortfolioSnapshot> snapshots;
    require_absent(calculate_maximum_drawdown(1000.0, snapshots));
}

TEST_CASE("Performance maximum drawdown includes initial equity as the first peak") {
    require_present(calculate_maximum_drawdown(1000.0, make_snapshots({900.0})), 0.10);
    require_present(calculate_maximum_drawdown(1000.0, make_snapshots({1000.0})), 0.0);
}

TEST_CASE("Performance maximum drawdown is zero on a monotonically rising path") {
    require_present(calculate_maximum_drawdown(
        1000.0, make_snapshots({1050.0, 1100.0, 1500.0})), 0.0);
}

TEST_CASE("Performance maximum drawdown tracks the largest peak-to-trough decline") {
    const auto snapshots = make_snapshots({1200.0, 900.0, 1400.0, 1120.0, 1500.0});
    // 1200 -> 900 = 25%; 1400 -> 1120 = 20%.
    require_present(calculate_maximum_drawdown(1000.0, snapshots), 0.25);
}

TEST_CASE("Performance maximum drawdown handles zero and negative equity") {
    require_present(calculate_maximum_drawdown(1000.0, make_snapshots({0.0})), 1.0);
    require_present(calculate_maximum_drawdown(1000.0, make_snapshots({-100.0})), 1.10);
}

TEST_CASE("Performance maximum drawdown survives insolvency and later recovery") {
    require_present(calculate_maximum_drawdown(
        1000.0, make_snapshots({800.0, 0.0, -100.0, 50.0})), 1.10);
}

TEST_CASE("Performance maximum drawdown can exceed one after a higher peak") {
    require_present(calculate_maximum_drawdown(
        1000.0, make_snapshots({2000.0, -1000.0, 500.0})), 1.5);
}

TEST_CASE("Performance maximum drawdown checks input validation") {
    REQUIRE_THROWS_AS(
        calculate_maximum_drawdown(0.0, make_snapshots({1000.0})),
        std::invalid_argument
    );
    auto snapshots = make_snapshots({1000.0, 800.0});
    std::reverse(snapshots.begin(), snapshots.end());
    REQUIRE_THROWS_AS(calculate_maximum_drawdown(1000.0, snapshots),
                      std::invalid_argument);
}

TEST_CASE("Performance maximum drawdown rejects nonfinite arithmetic overflow") {
    const double tiny = std::numeric_limits<double>::denorm_min();
    const double maximum = std::numeric_limits<double>::max();
    REQUIRE_THROWS_AS(
        calculate_maximum_drawdown(tiny, make_snapshots({-maximum})),
        std::overflow_error
    );
}

// -----------------------------------------------------------------------------
// Aggregate calculation and cross-component consistency
// -----------------------------------------------------------------------------

TEST_CASE("Performance aggregate matches every independent Oracle A metric") {
    const auto snapshots = make_snapshots({1000.0, 1050.0, 1029.0, 1131.9, 1131.9});
    const auto returns = build_return_series(1000.0, snapshots);
    const auto metrics = calculate_performance_metrics(1000.0, snapshots, returns);

    require_present(metrics.total_return, 0.1319);
    require_present(metrics.cagr, 514.1411225394515, 1e-10);
    require_present(metrics.annualized_volatility, 0.774441734412603);
    require_present(metrics.sharpe_ratio, 8.460287854927591);
    require_present(metrics.maximum_drawdown, 0.02);
}

TEST_CASE("Performance aggregate Oracle B preserves independently available metrics") {
    const auto snapshots = make_snapshots({800.0, 0.0, -100.0, 50.0});
    const auto returns = build_return_series(1000.0, snapshots);
    const auto metrics = calculate_performance_metrics(1000.0, snapshots, returns);

    require_present(metrics.total_return, -0.95);
    require_absent(metrics.cagr);
    require_absent(metrics.annualized_volatility);
    require_absent(metrics.sharpe_ratio);
    require_present(metrics.maximum_drawdown, 1.10);
}

TEST_CASE("Performance aggregate returns all unavailable metrics for empty history") {
    const std::vector<PortfolioSnapshot> snapshots;
    const ReturnSeries returns;
    const auto metrics = calculate_performance_metrics(1000.0, snapshots, returns);

    require_absent(metrics.total_return);
    require_absent(metrics.cagr);
    require_absent(metrics.annualized_volatility);
    require_absent(metrics.sharpe_ratio);
    require_absent(metrics.maximum_drawdown);
}

TEST_CASE("Performance aggregate handles one completed snapshot") {
    const auto snapshots = make_snapshots({1100.0});
    const auto returns = build_return_series(1000.0, snapshots);
    const auto metrics = calculate_performance_metrics(1000.0, snapshots, returns, 1.0);

    require_present(metrics.total_return, 0.10);
    require_present(metrics.cagr, 0.10);
    require_absent(metrics.annualized_volatility);
    require_absent(metrics.sharpe_ratio);
    require_present(metrics.maximum_drawdown, 0.0);
}

TEST_CASE("Performance aggregate agrees with all five individual functions") {
    for (const auto& snapshots : {
        make_snapshots({1000.0, 1050.0, 1029.0, 1131.9, 1131.9}),
        make_snapshots({800.0, 0.0, -100.0, 50.0}),
        make_snapshots({1000.0, 1000.0, 1000.0}),
        make_snapshots({1000.0}),
        make_snapshots({})
    }) {
        const auto returns = build_return_series(1000.0, snapshots);
        require_matches_individual_metrics(1000.0, snapshots, returns);
    }
}

TEST_CASE("Performance aggregate matches individual metrics at custom rates") {
    const auto snapshots = make_snapshots({1000.0, 1050.0, 1029.0, 1131.9, 1131.9});
    const auto returns = build_return_series(1000.0, snapshots);
    require_matches_individual_metrics(1000.0, snapshots, returns, 252.0, 0.04);
    require_matches_individual_metrics(1000.0, snapshots, returns, 12.0, -0.02);
}

TEST_CASE("Performance aggregate rejects mismatching history counts") {
    const auto snapshots = make_snapshots({1000.0, 1010.0});
    const auto returns = build_return_series(1000.0, snapshots);
    const ReturnSeries shorter{returns.begin(), returns.begin() + 1};
    REQUIRE_THROWS_AS(
        calculate_performance_metrics(1000.0, snapshots, shorter),
        std::invalid_argument
    );
    const std::vector<PortfolioSnapshot> empty;
    REQUIRE_THROWS_AS(
        calculate_performance_metrics(1000.0, empty, returns), std::invalid_argument
    );
}

TEST_CASE("Performance aggregate rejects mismatching observation dates") {
    const auto snapshots = make_snapshots({1000.0, 1010.0});
    auto returns = build_return_series(1000.0, snapshots);
    returns[1].session_date = session_date(10);
    REQUIRE_THROWS_AS(
        calculate_performance_metrics(1000.0, snapshots, returns),
        std::invalid_argument
    );
}

TEST_CASE("Performance aggregate rejects unordered dates even if histories match") {
    auto snapshots = make_snapshots({1000.0, 1010.0});
    std::reverse(snapshots.begin(), snapshots.end());
    auto returns = make_returns({0.0, 0.01});
    returns[0].session_date = snapshots[0].session_date();
    returns[1].session_date = snapshots[1].session_date();
    REQUIRE_THROWS_AS(
        calculate_performance_metrics(1000.0, snapshots, returns),
        std::invalid_argument
    );
}

TEST_CASE("Performance aggregate rejects invalid common settings") {
    const auto snapshots = make_snapshots({1000.0, 1100.0});
    const auto returns = build_return_series(1000.0, snapshots);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    REQUIRE_THROWS_AS(
        calculate_performance_metrics(0.0, snapshots, returns), std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        calculate_performance_metrics(1000.0, snapshots, returns, 0.0),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        calculate_performance_metrics(1000.0, snapshots, returns, 252.0, -1.0),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        calculate_performance_metrics(1000.0, snapshots, returns, 252.0, nan),
        std::invalid_argument
    );
}

TEST_CASE("Performance aggregate never drops missing simple returns") {
    const auto snapshots = make_snapshots({900.0, 800.0, 700.0});
    auto returns = build_return_series(1000.0, snapshots);
    returns[1].simple_return = std::nullopt;
    const auto metrics = calculate_performance_metrics(1000.0, snapshots, returns);
    require_present(metrics.total_return, -0.30);
    require_present(metrics.cagr, std::expm1(std::log(0.7) * 252.0 / 3.0));
    require_absent(metrics.annualized_volatility);
    require_absent(metrics.sharpe_ratio);
    require_present(metrics.maximum_drawdown, 0.30);
}

TEST_CASE("Performance aggregate validates returns even when CAGR is unavailable") {
    const auto snapshots = make_snapshots({800.0, 0.0, -100.0, 50.0});
    auto returns = build_return_series(1000.0, snapshots);
    returns[3].simple_return = std::numeric_limits<double>::infinity();
    REQUIRE_THROWS_AS(
        calculate_performance_metrics(1000.0, snapshots, returns),
        std::invalid_argument
    );
}

TEST_CASE("Performance aggregate accepts sparse calendar dates without interpolation") {
    std::vector<PortfolioSnapshot> snapshots;
    snapshots.push_back(make_snapshot(0, 1000.0));
    snapshots.push_back(make_snapshot(20, 1050.0));
    snapshots.push_back(make_snapshot(40, 1029.0));
    const auto returns = build_return_series(1000.0, snapshots);
    require_matches_individual_metrics(1000.0, snapshots, returns);
}

TEST_CASE("Performance aggregate is deterministic and does not mutate its inputs") {
    const auto snapshots = make_snapshots({1000.0, 1050.0, 1029.0, 1131.9, 1131.9});
    const auto returns = build_return_series(1000.0, snapshots);
    const auto original_returns = returns;
    const auto first = calculate_performance_metrics(1000.0, snapshots, returns);
    const auto second = calculate_performance_metrics(1000.0, snapshots, returns);
    require_equivalent(first.total_return, second.total_return);
    require_equivalent(first.cagr, second.cagr);
    require_equivalent(first.annualized_volatility, second.annualized_volatility);
    require_equivalent(first.sharpe_ratio, second.sharpe_ratio);
    require_equivalent(first.maximum_drawdown, second.maximum_drawdown);
    REQUIRE(returns.size() == original_returns.size());
    for (std::size_t i = 0; i < returns.size(); ++i) {
        REQUIRE(returns[i].session_date == original_returns[i].session_date);
        REQUIRE(returns[i].simple_return == original_returns[i].simple_return);
        REQUIRE(returns[i].log_return == original_returns[i].log_return);
    }
}

TEST_CASE("Performance aggregate supports a long deterministic observation history") {
    constexpr int count = 5000;
    const auto snapshots = [&] {
        std::vector<PortfolioSnapshot> result;
        result.reserve(count);
        for (int i = 0; i < count; ++i) {
            result.push_back(make_snapshot(i, 1000.0));
        }
        return result;
    }();
    const auto returns = build_return_series(1000.0, snapshots);
    const auto metrics = calculate_performance_metrics(1000.0, snapshots, returns);
    require_present(metrics.total_return, 0.0);
    require_present(metrics.cagr, 0.0);
    require_present(metrics.annualized_volatility, 0.0);
    require_absent(metrics.sharpe_ratio);
    require_present(metrics.maximum_drawdown, 0.0);
}
