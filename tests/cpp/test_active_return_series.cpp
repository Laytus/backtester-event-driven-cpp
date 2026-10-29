#include "backtester/performance/active_return_series.hpp"
#include "backtester/performance/return_series.hpp"
#include "backtester/portfolio/portfolio.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

using backtester::market::SessionDate;
using backtester::performance::ActiveReturnSeries;
using backtester::performance::ReturnSeries;
using backtester::performance::build_active_return_series;
using backtester::performance::build_return_series;
using backtester::portfolio::PortfolioSnapshot;

SessionDate session_date(int offset) {
    const std::chrono::sys_days first{
        std::chrono::year{2026} / std::chrono::September / 14
    };
    return SessionDate{first + std::chrono::days{offset}};
}

ReturnSeries make_returns(
    std::initializer_list<std::optional<double>> simple_returns
) {
    ReturnSeries observations;
    observations.reserve(simple_returns.size());
    int offset = 0;

    for (const auto& simple_return : simple_returns) {
        observations.push_back({
            session_date(offset++), simple_return, std::nullopt
        });
    }
    return observations;
}

std::vector<PortfolioSnapshot> make_snapshots(
    std::initializer_list<double> equities
) {
    std::vector<PortfolioSnapshot> snapshots;
    snapshots.reserve(equities.size());
    int offset = 0;

    for (double equity : equities) {
        snapshots.emplace_back(
            session_date(offset++), equity, 0, 0.0, equity
        );
    }
    return snapshots;
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
    REQUIRE(
        std::abs(actual - expected) <=
        std::max(absolute_tolerance, relative_tolerance * scale)
    );
}

void require_active(
    const ActiveReturnSeries& series,
    std::size_t index,
    int date_offset,
    std::optional<double> expected,
    double absolute_tolerance = 1e-12,
    double relative_tolerance = 1e-10
) {
    REQUIRE(index < series.size());
    REQUIRE(series[index].session_date == session_date(date_offset));
    REQUIRE(series[index].active_return.has_value() == expected.has_value());
    if (expected) {
        require_close(
            *series[index].active_return,
            *expected,
            absolute_tolerance,
            relative_tolerance
        );
    }
}

void require_equal_series(
    const ActiveReturnSeries& lhs,
    const ActiveReturnSeries& rhs
) {
    REQUIRE(lhs.size() == rhs.size());
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        REQUIRE(lhs[i].session_date == rhs[i].session_date);
        REQUIRE(lhs[i].active_return == rhs[i].active_return);
    }
}

}  // namespace

// -----------------------------------------------------------------------------
// Basic construction, Oracle C, and Oracle D
// -----------------------------------------------------------------------------

TEST_CASE("ActiveReturnSeries accepts two empty histories") {
    const ReturnSeries empty;
    const auto active = build_active_return_series(empty, empty);
    REQUIRE(active.empty());
}

TEST_CASE("ActiveReturnSeries computes a single dated observation") {
    const auto active = build_active_return_series(
        make_returns({0.05}), make_returns({0.02})
    );
    REQUIRE(active.size() == 1);
    require_active(active, 0, 0, 0.03);
}

TEST_CASE("ActiveReturnSeries processes every observation including the last") {
    const auto active = build_active_return_series(
        make_returns({0.02, -0.04, 0.10, 0.0}),
        make_returns({0.01, 0.03, -0.02, -0.05})
    );
    REQUIRE(active.size() == 4);
    require_active(active, 0, 0, 0.01);
    require_active(active, 1, 1, -0.07);
    require_active(active, 2, 2, 0.12);
    require_active(active, 3, 3, 0.05);
}

TEST_CASE("ActiveReturnSeries Oracle C matches flat versus Buy and Hold") {
    // Flat strategy returns 0 on each completed session.
    // Buy-and-Hold equity: 1000, 1100, 1200, 900.
    const auto active = build_active_return_series(
        make_returns({0.0, 0.0, 0.0, 0.0}),
        make_returns({0.0, 0.10, 1.0 / 11.0, -0.25})
    );
    REQUIRE(active.size() == 4);
    require_active(active, 0, 0, 0.0);
    require_active(active, 1, 1, -0.10);
    require_active(active, 2, 2, -1.0 / 11.0);
    require_active(active, 3, 3, 0.25);
}

TEST_CASE("ActiveReturnSeries Oracle D matches the approved five-pair reference") {
    const auto active = build_active_return_series(
        make_returns({0.00, 0.01, 0.01, 0.02, 0.01}),
        make_returns({-0.02, -0.01, 0.00, 0.01, 0.02})
    );
    REQUIRE(active.size() == 5);
    require_active(active, 0, 0, 0.02);
    require_active(active, 1, 1, 0.02);
    require_active(active, 2, 2, 0.01);
    require_active(active, 3, 3, 0.01);
    require_active(active, 4, 4, -0.01);
}

TEST_CASE("ActiveReturnSeries integrates with real ReturnSeries builders") {
    const auto strategy_snapshots = make_snapshots({
        1000.0, 1000.0, 1000.0, 1000.0
    });
    const auto benchmark_snapshots = make_snapshots({
        1000.0, 1100.0, 1200.0, 900.0
    });
    const auto strategy = build_return_series(1000.0, strategy_snapshots);
    const auto benchmark = build_return_series(1000.0, benchmark_snapshots);
    const auto active = build_active_return_series(strategy, benchmark);

    REQUIRE(active.size() == 4);
    require_active(active, 0, 0, 0.0);
    require_active(active, 1, 1, -0.10);
    require_active(active, 2, 2, -1.0 / 11.0);
    require_active(active, 3, 3, 0.25);
}

TEST_CASE("ActiveReturnSeries preserves explicit zero differences") {
    const auto active = build_active_return_series(
        make_returns({0.0, -0.05, 0.07}),
        make_returns({0.0, -0.05, 0.07})
    );
    REQUIRE(active.size() == 3);
    for (std::size_t i = 0; i < active.size(); ++i) {
        REQUIRE(active[i].active_return.has_value());
        REQUIRE(*active[i].active_return == 0.0);
    }
}

TEST_CASE("ActiveReturnSeries supports a return below minus one") {
    const auto active = build_active_return_series(
        make_returns({-1.2, 0.5}), make_returns({-0.2, -1.5})
    );
    REQUIRE(active.size() == 2);
    require_active(active, 0, 0, -1.0);
    require_active(active, 1, 1, 2.0);
}

// -----------------------------------------------------------------------------
// Availability and independence from log returns
// -----------------------------------------------------------------------------

TEST_CASE("ActiveReturnSeries preserves unavailable strategy returns") {
    const auto active = build_active_return_series(
        make_returns({0.05, std::nullopt, -0.02}),
        make_returns({0.01, 0.03, -0.04})
    );
    REQUIRE(active.size() == 3);
    require_active(active, 0, 0, 0.04);
    require_active(active, 1, 1, std::nullopt);
    require_active(active, 2, 2, 0.02);
}

TEST_CASE("ActiveReturnSeries preserves unavailable benchmark returns") {
    const auto active = build_active_return_series(
        make_returns({0.05, 0.02, -0.02}),
        make_returns({0.01, std::nullopt, -0.04})
    );
    REQUIRE(active.size() == 3);
    require_active(active, 0, 0, 0.04);
    require_active(active, 1, 1, std::nullopt);
    require_active(active, 2, 2, 0.02);
}

TEST_CASE("ActiveReturnSeries preserves dates when both returns are missing") {
    const auto active = build_active_return_series(
        make_returns({std::nullopt, 0.1, std::nullopt}),
        make_returns({std::nullopt, 0.0, 0.01})
    );
    REQUIRE(active.size() == 3);
    require_active(active, 0, 0, std::nullopt);
    require_active(active, 1, 1, 0.1);
    require_active(active, 2, 2, std::nullopt);
}

TEST_CASE("ActiveReturnSeries distinguishes a zero return from nullopt") {
    const auto active = build_active_return_series(
        make_returns({0.0, std::nullopt}),
        make_returns({0.05, 0.05})
    );
    REQUIRE(active.size() == 2);
    require_active(active, 0, 0, -0.05);
    require_active(active, 1, 1, std::nullopt);
}

TEST_CASE("ActiveReturnSeries does not substitute logarithmic returns") {
    auto strategy = make_returns({std::nullopt, 0.1});
    auto benchmark = make_returns({0.0, 0.0});
    strategy[0].log_return = 0.5;
    strategy[1].log_return = -0.5;
    benchmark[0].log_return = 0.25;

    const auto active = build_active_return_series(strategy, benchmark);
    REQUIRE(active.size() == 2);
    require_active(active, 0, 0, std::nullopt);
    require_active(active, 1, 1, 0.1);
}

TEST_CASE("ActiveReturnSeries ignores nonfinite logarithmic returns by contract") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    auto strategy = make_returns({0.03, 0.04});
    auto benchmark = make_returns({0.01, std::nullopt});
    strategy[0].log_return = nan;
    benchmark[1].log_return = std::numeric_limits<double>::infinity();

    const auto active = build_active_return_series(strategy, benchmark);
    REQUIRE(active.size() == 2);
    require_active(active, 0, 0, 0.02);
    require_active(active, 1, 1, std::nullopt);
}

TEST_CASE("ActiveReturnSeries does not create extra anchor observations") {
    const auto active = build_active_return_series(
        make_returns({0.03, 0.02}), make_returns({0.01, -0.04})
    );
    REQUIRE(active.size() == 2);
    REQUIRE(active.front().session_date == session_date(0));
    REQUIRE(active.back().session_date == session_date(1));
}

// -----------------------------------------------------------------------------
// Alignment and chronological validation
// -----------------------------------------------------------------------------

TEST_CASE("ActiveReturnSeries rejects unequal lengths in either direction") {
    const auto one = make_returns({0.0});
    const auto two = make_returns({0.0, 0.1});
    const ReturnSeries empty;
    REQUIRE_THROWS_AS(build_active_return_series(one, two), std::invalid_argument);
    REQUIRE_THROWS_AS(build_active_return_series(two, one), std::invalid_argument);
    REQUIRE_THROWS_AS(build_active_return_series(empty, one), std::invalid_argument);
    REQUIRE_THROWS_AS(build_active_return_series(one, empty), std::invalid_argument);
}

TEST_CASE("ActiveReturnSeries rejects mismatched dates at the first observation") {
    auto strategy = make_returns({0.0, 0.1});
    auto benchmark = make_returns({0.0, 0.1});
    benchmark[0].session_date = session_date(-1);
    REQUIRE_THROWS_AS(
        build_active_return_series(strategy, benchmark), std::invalid_argument
    );
}

TEST_CASE("ActiveReturnSeries rejects mismatched dates at the final observation") {
    auto strategy = make_returns({0.0, 0.1});
    auto benchmark = make_returns({0.0, 0.1});
    benchmark[1].session_date = session_date(5);
    REQUIRE_THROWS_AS(
        build_active_return_series(strategy, benchmark), std::invalid_argument
    );
}

TEST_CASE("ActiveReturnSeries rejects duplicated aligned session dates") {
    auto strategy = make_returns({0.0, 0.1, 0.2});
    auto benchmark = make_returns({0.0, 0.1, 0.2});
    strategy[1].session_date = session_date(0);
    benchmark[1].session_date = session_date(0);
    REQUIRE_THROWS_AS(
        build_active_return_series(strategy, benchmark), std::invalid_argument
    );
}

TEST_CASE("ActiveReturnSeries rejects descending aligned session dates") {
    auto strategy = make_returns({0.0, 0.1, 0.2});
    auto benchmark = make_returns({0.0, 0.1, 0.2});
    std::reverse(strategy.begin(), strategy.end());
    std::reverse(benchmark.begin(), benchmark.end());
    REQUIRE_THROWS_AS(
        build_active_return_series(strategy, benchmark), std::invalid_argument
    );
}

TEST_CASE("ActiveReturnSeries rejects an invalid strategy session date") {
    auto strategy = make_returns({0.01});
    const auto benchmark = make_returns({0.02});
    strategy[0].session_date = SessionDate{
        std::chrono::year{2026}, std::chrono::month{13}, std::chrono::day{1}
    };
    REQUIRE_THROWS_AS(
        build_active_return_series(strategy, benchmark), std::invalid_argument
    );
}

TEST_CASE("ActiveReturnSeries rejects an invalid benchmark session date") {
    const auto strategy = make_returns({0.01});
    auto benchmark = make_returns({0.02});
    benchmark[0].session_date = SessionDate{
        std::chrono::year{2026}, std::chrono::month{9}, std::chrono::day{32}
    };
    REQUIRE_THROWS_AS(
        build_active_return_series(strategy, benchmark), std::invalid_argument
    );
}

TEST_CASE("ActiveReturnSeries accepts aligned dates with calendar gaps") {
    auto strategy = make_returns({0.02, 0.04, -0.01});
    auto benchmark = make_returns({0.01, 0.01, 0.03});
    for (auto* series : {&strategy, &benchmark}) {
        (*series)[1].session_date = session_date(10);
        (*series)[2].session_date = session_date(30);
    }
    const auto active = build_active_return_series(strategy, benchmark);
    REQUIRE(active.size() == 3);
    require_active(active, 0, 0, 0.01);
    require_active(active, 1, 10, 0.03);
    require_active(active, 2, 30, -0.04);
}

TEST_CASE("ActiveReturnSeries accepts matched span subranges") {
    const auto strategy = make_returns({10.0, 0.04, 0.03, -10.0});
    const auto benchmark = make_returns({-10.0, 0.01, -0.02, 10.0});
    const auto active = build_active_return_series(
        std::span<const backtester::performance::ReturnObservation>{strategy}.subspan(1, 2),
        std::span<const backtester::performance::ReturnObservation>{benchmark}.subspan(1, 2)
    );
    REQUIRE(active.size() == 2);
    require_active(active, 0, 1, 0.03);
    require_active(active, 1, 2, 0.05);
}

// -----------------------------------------------------------------------------
// Numerical validation and overflow
// -----------------------------------------------------------------------------

TEST_CASE("ActiveReturnSeries rejects NaN in strategy simple returns") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (std::size_t index = 0; index < 3; ++index) {
        auto strategy = make_returns({0.0, 0.01, 0.02});
        const auto benchmark = make_returns({0.0, 0.01, 0.02});
        strategy[index].simple_return = nan;
        REQUIRE_THROWS_AS(
            build_active_return_series(strategy, benchmark), std::invalid_argument
        );
    }
}

TEST_CASE("ActiveReturnSeries rejects NaN in benchmark simple returns") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (std::size_t index = 0; index < 3; ++index) {
        const auto strategy = make_returns({0.0, 0.01, 0.02});
        auto benchmark = make_returns({0.0, 0.01, 0.02});
        benchmark[index].simple_return = nan;
        REQUIRE_THROWS_AS(
            build_active_return_series(strategy, benchmark), std::invalid_argument
        );
    }
}

TEST_CASE("ActiveReturnSeries rejects signed infinity in either input") {
    const double inf = std::numeric_limits<double>::infinity();
    for (const double invalid : {inf, -inf}) {
        REQUIRE_THROWS_AS(
            build_active_return_series(
                make_returns({0.0, invalid}), make_returns({0.0, 0.0})
            ),
            std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            build_active_return_series(
                make_returns({0.0, 0.0}), make_returns({0.0, invalid})
            ),
            std::invalid_argument
        );
    }
}

TEST_CASE("ActiveReturnSeries validates finite values when the other is missing") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    REQUIRE_THROWS_AS(
        build_active_return_series(
            make_returns({std::nullopt}), make_returns({nan})
        ),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        build_active_return_series(
            make_returns({inf}), make_returns({std::nullopt})
        ),
        std::invalid_argument
    );
}

TEST_CASE("ActiveReturnSeries rejects positive and negative difference overflow") {
    const double max = std::numeric_limits<double>::max();
    REQUIRE_THROWS_AS(
        build_active_return_series(make_returns({max}), make_returns({-max})),
        std::overflow_error
    );
    REQUIRE_THROWS_AS(
        build_active_return_series(make_returns({-max}), make_returns({max})),
        std::overflow_error
    );
}

TEST_CASE("ActiveReturnSeries preserves representable extreme differences") {
    const double max = std::numeric_limits<double>::max();
    const auto active = build_active_return_series(
        make_returns({max, -max, max, -max}),
        make_returns({0.0, 0.0, max, -max})
    );
    REQUIRE(active.size() == 4);
    REQUIRE(*active[0].active_return == max);
    REQUIRE(*active[1].active_return == -max);
    REQUIRE(*active[2].active_return == 0.0);
    REQUIRE(*active[3].active_return == 0.0);
}

TEST_CASE("ActiveReturnSeries preserves subnormal finite differences") {
    const double tiny = std::numeric_limits<double>::denorm_min();
    const auto active = build_active_return_series(
        make_returns({tiny, 0.0, -tiny}),
        make_returns({0.0, tiny, 0.0})
    );
    REQUIRE(active.size() == 3);
    REQUIRE(*active[0].active_return == tiny);
    REQUIRE(*active[1].active_return == -tiny);
    REQUIRE(*active[2].active_return == -tiny);
}

TEST_CASE("ActiveReturnSeries retains differences near a large common value") {
    const double anchor = 1e16;
    const double next = std::nextafter(anchor, std::numeric_limits<double>::infinity());
    const auto active = build_active_return_series(
        make_returns({next}), make_returns({anchor})
    );
    REQUIRE(active.size() == 1);
    REQUIRE(*active.front().active_return == next - anchor);
}

TEST_CASE("ActiveReturnSeries supports large but representable finite differences") {
    const auto active = build_active_return_series(
        make_returns({1e300, -1e300}),
        make_returns({-1e300, 1e300})
    );
    REQUIRE(active.size() == 2);
    require_active(active, 0, 0, 2e300, 0.0, 1e-12);
    require_active(active, 1, 1, -2e300, 0.0, 1e-12);
}

// -----------------------------------------------------------------------------
// Determinism, ownership, and long histories
// -----------------------------------------------------------------------------

TEST_CASE("ActiveReturnSeries does not modify its inputs") {
    auto strategy = make_returns({0.0, 0.05, std::nullopt, -0.01});
    auto benchmark = make_returns({0.0, -0.02, 0.03, std::nullopt});
    strategy[0].log_return = 0.0;
    benchmark[2].log_return = std::log1p(0.03);
    const auto original_strategy = strategy;
    const auto original_benchmark = benchmark;

    (void)build_active_return_series(strategy, benchmark);

    for (std::size_t i = 0; i < strategy.size(); ++i) {
        REQUIRE(strategy[i].session_date == original_strategy[i].session_date);
        REQUIRE(strategy[i].simple_return == original_strategy[i].simple_return);
        REQUIRE(strategy[i].log_return == original_strategy[i].log_return);
        REQUIRE(benchmark[i].session_date == original_benchmark[i].session_date);
        REQUIRE(benchmark[i].simple_return == original_benchmark[i].simple_return);
        REQUIRE(benchmark[i].log_return == original_benchmark[i].log_return);
    }
}

TEST_CASE("ActiveReturnSeries repeated construction is deterministic") {
    const auto strategy = make_returns({0.0, 0.05, std::nullopt, -0.02, 0.1});
    const auto benchmark = make_returns({0.01, -0.01, 0.03, 0.02, 0.0});
    const auto expected = build_active_return_series(strategy, benchmark);

    for (int iteration = 0; iteration < 20; ++iteration) {
        const auto actual = build_active_return_series(strategy, benchmark);
        require_equal_series(actual, expected);
    }
}

TEST_CASE("ActiveReturnSeries can be built from independent input vectors") {
    auto strategy = make_returns({0.01, 0.02});
    auto benchmark = make_returns({0.0, 0.01});
    const auto first = build_active_return_series(strategy, benchmark);
    strategy[0].simple_return = 0.20;
    benchmark[1].simple_return = -0.02;
    const auto second = build_active_return_series(strategy, benchmark);

    require_active(first, 0, 0, 0.01);
    require_active(first, 1, 1, 0.01);
    require_active(second, 0, 0, 0.20);
    require_active(second, 1, 1, 0.04);
}

TEST_CASE("ActiveReturnSeries supports a long chronological history") {
    constexpr int count = 5000;
    ReturnSeries strategy;
    ReturnSeries benchmark;
    strategy.reserve(count);
    benchmark.reserve(count);

    for (int i = 0; i < count; ++i) {
        const double strategy_return = (i % 2 == 0) ? 0.02 : -0.01;
        const double benchmark_return = (i % 3 == 0) ? 0.01 : -0.02;
        strategy.push_back({session_date(i), strategy_return, std::nullopt});
        benchmark.push_back({session_date(i), benchmark_return, std::nullopt});
    }

    const auto active = build_active_return_series(strategy, benchmark);
    REQUIRE(active.size() == static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        REQUIRE(active[static_cast<std::size_t>(i)].session_date == session_date(i));
        const double s = (i % 2 == 0) ? 0.02 : -0.01;
        const double b = (i % 3 == 0) ? 0.01 : -0.02;
        require_close(*active[static_cast<std::size_t>(i)].active_return, s - b);
    }
}
