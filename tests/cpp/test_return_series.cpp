#include "backtester/performance/return_series.hpp"

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
using backtester::performance::ReturnObservation;
using backtester::performance::ReturnSeries;
using backtester::performance::build_return_series;
using backtester::portfolio::PortfolioSnapshot;

SessionDate session_date(int day_offset) {
    const std::chrono::sys_days first{
        std::chrono::year{2026} / std::chrono::September / 14
    };
    return SessionDate{first + std::chrono::days{day_offset}};
}

// Zero position and zero market value keep the snapshot internally consistent:
// equity = cash + market value. These are synthetic analytics fixtures, not
// snapshots generated through the cash-only MVP's trading lifecycle.
PortfolioSnapshot make_snapshot(int day_offset, double equity) {
    return PortfolioSnapshot{session_date(day_offset), equity, 0, 0.0, equity};
}

std::vector<PortfolioSnapshot> make_snapshots(
    std::initializer_list<double> equities
) {
    std::vector<PortfolioSnapshot> snapshots;
    snapshots.reserve(equities.size());
    int day_offset = 0;
    for (double equity : equities) {
        snapshots.push_back(make_snapshot(day_offset++, equity));
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
    const double scale = std::max(std::fabs(actual), std::fabs(expected));
    const double tolerance = std::max(
        absolute_tolerance, relative_tolerance * scale
    );
    REQUIRE(std::fabs(actual - expected) <= tolerance);
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

void require_same_observation(
    const ReturnObservation& actual,
    const ReturnObservation& expected
) {
    REQUIRE(actual.session_date == expected.session_date);
    REQUIRE(actual.simple_return.has_value() == expected.simple_return.has_value());
    REQUIRE(actual.log_return.has_value() == expected.log_return.has_value());
    if (expected.simple_return) {
        require_close(*actual.simple_return, *expected.simple_return);
    }
    if (expected.log_return) {
        require_close(*actual.log_return, *expected.log_return);
    }
}

}  // namespace

TEST_CASE("ReturnSeries rejects nonpositive initial equity") {
    const auto snapshots = make_snapshots({1000.0});
    for (double initial_equity : {0.0, -0.0, -1.0, -1000.0}) {
        REQUIRE_THROWS_AS(
            build_return_series(initial_equity, snapshots), std::invalid_argument
        );
    }
}

TEST_CASE("ReturnSeries rejects nonfinite initial equity") {
    const auto snapshots = make_snapshots({1000.0});
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (double initial_equity : {nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            build_return_series(initial_equity, snapshots), std::invalid_argument
        );
    }
}

TEST_CASE("ReturnSeries checks initial equity even for an empty history") {
    const std::vector<PortfolioSnapshot> empty;
    REQUIRE_THROWS_AS(build_return_series(0.0, empty), std::invalid_argument);
    REQUIRE_THROWS_AS(
        build_return_series(std::numeric_limits<double>::infinity(), empty),
        std::invalid_argument
    );
}

TEST_CASE("ReturnSeries accepts an empty history without inventing an anchor") {
    const std::vector<PortfolioSnapshot> empty;
    const ReturnSeries result = build_return_series(1000.0, empty);
    REQUIRE(result.empty());
}

TEST_CASE("ReturnSeries creates one observation for one snapshot") {
    const auto snapshots = make_snapshots({1050.0});
    const auto result = build_return_series(1000.0, snapshots);
    REQUIRE(result.size() == 1);
    REQUIRE(result.front().session_date == session_date(0));
    require_present(result.front().simple_return, 0.05);
    require_present(result.front().log_return, 0.048790164169432);
}

TEST_CASE("ReturnSeries includes the initial-to-first-snapshot return") {
    const auto snapshots = make_snapshots({900.0, 990.0});
    const auto result = build_return_series(1000.0, snapshots);
    REQUIRE(result.size() == 2);
    require_present(result[0].simple_return, -0.10);
    require_present(result[1].simple_return, 0.10);
    require_present(result[0].log_return, -0.105360515657826);
    require_present(result[1].log_return, 0.095310179804325);
}

TEST_CASE("ReturnSeries Oracle A matches independent simple and log returns") {
    const auto snapshots = make_snapshots({
        1000.0, 1050.0, 1029.0, 1131.9, 1131.9
    });
    constexpr std::array<double, 5> simple{
        0.0, 0.05, -0.02, 0.10, 0.0
    };
    constexpr std::array<double, 5> logarithmic{
        0.0,
        0.048790164169432,
        -0.020202707317519,
        0.095310179804325,
        0.0
    };

    const auto result = build_return_series(1000.0, snapshots);
    REQUIRE(result.size() == simple.size());
    for (std::size_t i = 0; i < simple.size(); ++i) {
        REQUIRE(result[i].session_date == session_date(static_cast<int>(i)));
        require_present(result[i].simple_return, simple[i]);
        require_present(result[i].log_return, logarithmic[i]);
    }
}

TEST_CASE("ReturnSeries Oracle A compounds to the independently known endpoint") {
    const auto result = build_return_series(
        1000.0, make_snapshots({1000.0, 1050.0, 1029.0, 1131.9, 1131.9})
    );
    double compounded = 1.0;
    double sum_of_logs = 0.0;
    for (const auto& observation : result) {
        REQUIRE(observation.simple_return.has_value());
        REQUIRE(observation.log_return.has_value());
        compounded *= 1.0 + *observation.simple_return;
        sum_of_logs += *observation.log_return;
    }
    require_close(compounded, 1.1319);
    require_close(sum_of_logs, 0.123897636656257, 1e-12, 1e-10);
}

TEST_CASE("ReturnSeries Oracle B preserves zero, negative, and recovering equity") {
    const auto snapshots = make_snapshots({800.0, 0.0, -100.0, 50.0});
    const auto result = build_return_series(1000.0, snapshots);
    REQUIRE(result.size() == 4);
    for (std::size_t i = 0; i < result.size(); ++i) {
        REQUIRE(result[i].session_date == session_date(static_cast<int>(i)));
    }
    require_present(result[0].simple_return, -0.20);
    require_present(result[0].log_return, -0.223143551314210);
    require_present(result[1].simple_return, -1.0);
    require_absent(result[1].log_return);
    require_absent(result[2].simple_return);
    require_absent(result[2].log_return);
    require_absent(result[3].simple_return);
    require_absent(result[3].log_return);
}

TEST_CASE("ReturnSeries preserves a simple return below minus one") {
    const auto result = build_return_series(1000.0, make_snapshots({-200.0}));
    REQUIRE(result.size() == 1);
    require_present(result[0].simple_return, -1.2);
    require_absent(result[0].log_return);
}

TEST_CASE("ReturnSeries does not define logarithmic return at zero equity") {
    const auto result = build_return_series(1000.0, make_snapshots({0.0}));
    require_present(result[0].simple_return, -1.0);
    require_absent(result[0].log_return);
}

TEST_CASE("ReturnSeries does not define returns after a zero equity anchor") {
    const auto result = build_return_series(1000.0, make_snapshots({0.0, 500.0}));
    REQUIRE(result.size() == 2);
    require_present(result[0].simple_return, -1.0);
    require_absent(result[0].log_return);
    require_absent(result[1].simple_return);
    require_absent(result[1].log_return);
}

TEST_CASE("ReturnSeries does not define returns after a negative equity anchor") {
    const auto result = build_return_series(
        1000.0, make_snapshots({-100.0, -50.0, 300.0})
    );
    REQUIRE(result.size() == 3);
    require_present(result[0].simple_return, -1.1);
    require_absent(result[0].log_return);
    for (std::size_t i = 1; i < result.size(); ++i) {
        require_absent(result[i].simple_return);
        require_absent(result[i].log_return);
    }
}

TEST_CASE("ReturnSeries can resume available returns after equity recovers") {
    const auto result = build_return_series(
        1000.0, make_snapshots({0.0, 50.0, 55.0})
    );
    REQUIRE(result.size() == 3);
    require_absent(result[1].simple_return);
    require_absent(result[1].log_return);
    require_present(result[2].simple_return, 0.10);
    require_present(result[2].log_return, 0.095310179804325);
}

TEST_CASE("ReturnSeries retains a valid zero return for unchanged positive equity") {
    const auto result = build_return_series(
        1000.0, make_snapshots({1000.0, 1000.0, 1000.0})
    );
    REQUIRE(result.size() == 3);
    for (const auto& observation : result) {
        require_present(observation.simple_return, 0.0);
        require_present(observation.log_return, 0.0);
    }
}

TEST_CASE("ReturnSeries rejects duplicate snapshot dates") {
    std::vector<PortfolioSnapshot> snapshots{
        make_snapshot(0, 1000.0), make_snapshot(0, 1100.0)
    };
    REQUIRE_THROWS_AS(
        build_return_series(1000.0, snapshots), std::invalid_argument
    );
}

TEST_CASE("ReturnSeries rejects decreasing snapshot dates") {
    std::vector<PortfolioSnapshot> snapshots{
        make_snapshot(2, 1000.0), make_snapshot(1, 1100.0)
    };
    REQUIRE_THROWS_AS(
        build_return_series(1000.0, snapshots), std::invalid_argument
    );
}

TEST_CASE("PortfolioSnapshot rejects invalid dates before ReturnSeries") {
    const SessionDate invalid{
        std::chrono::year{2026} / std::chrono::February / 30
    };
    REQUIRE_FALSE(invalid.ok());
    // The domain object rejects malformed input before ReturnSeries can read it.
    REQUIRE_THROWS_AS(
        (PortfolioSnapshot{invalid, 1000.0, 0, 0.0, 1000.0}),
        std::invalid_argument
    );
}

TEST_CASE("ReturnSeries permits missing sessions without interpolation") {
    const std::vector<PortfolioSnapshot> snapshots{
        make_snapshot(0, 1000.0),
        make_snapshot(3, 1100.0),
        make_snapshot(14, 990.0)
    };
    const auto result = build_return_series(1000.0, snapshots);
    REQUIRE(result.size() == snapshots.size());
    REQUIRE(result[0].session_date == session_date(0));
    REQUIRE(result[1].session_date == session_date(3));
    REQUIRE(result[2].session_date == session_date(14));
    require_present(result[1].simple_return, 0.10);
    require_present(result[2].simple_return, -0.10);
}

TEST_CASE("PortfolioSnapshot rejects nonfinite equity before ReturnSeries") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (double invalid : {nan, inf, -inf}) {
        // A nonfinite snapshot cannot be constructed through the public API.
        REQUIRE_THROWS_AS(
            (PortfolioSnapshot{session_date(1), invalid, 0, 0.0, invalid}),
            std::invalid_argument
        );
    }
}

TEST_CASE("PortfolioSnapshot rejects nonfinite first-observation equity") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    // The first snapshot is subject to the same domain validation.
    REQUIRE_THROWS_AS(
        (PortfolioSnapshot{session_date(0), nan, 0, 0.0, nan}),
        std::invalid_argument
    );
}

TEST_CASE("ReturnSeries rejects overflowing positive simple returns") {
    const double tiny = std::numeric_limits<double>::denorm_min();
    const double huge = std::numeric_limits<double>::max();
    const auto snapshots = make_snapshots({huge});
    REQUIRE_THROWS_AS(
        build_return_series(tiny, snapshots), std::overflow_error
    );
}

TEST_CASE("ReturnSeries rejects overflowing negative simple returns") {
    const double tiny = std::numeric_limits<double>::denorm_min();
    const double huge = std::numeric_limits<double>::max();
    const auto snapshots = make_snapshots({-huge});
    REQUIRE_THROWS_AS(
        build_return_series(tiny, snapshots), std::overflow_error
    );
}

TEST_CASE("ReturnSeries handles the smallest positive representable equity") {
    const double tiny = std::numeric_limits<double>::denorm_min();
    const auto result = build_return_series(1.0, make_snapshots({tiny}));
    REQUIRE(result.size() == 1);
    require_present(result[0].simple_return, -1.0);
    REQUIRE(result[0].log_return.has_value());
    REQUIRE(std::isfinite(*result[0].log_return));
    // log(2^-1074) is a finite value near -744.44, unlike log1p(-1).
    require_close(*result[0].log_return, -744.4400719213812, 1e-10, 1e-12);
}

TEST_CASE("ReturnSeries supports positive extreme-scale decreases") {
    const double huge = std::numeric_limits<double>::max();
    const double tiny = std::numeric_limits<double>::min();
    const auto result = build_return_series(huge, make_snapshots({tiny}));
    require_present(result[0].simple_return, -1.0);
    REQUIRE(result[0].log_return.has_value());
    REQUIRE(std::isfinite(*result[0].log_return));
    REQUIRE(*result[0].log_return < -1400.0);
}

TEST_CASE("ReturnSeries detects representable near-one positive changes") {
    const double next = std::nextafter(1.0, 2.0);
    const double epsilon = std::numeric_limits<double>::epsilon();
    const auto result = build_return_series(1.0, make_snapshots({next}));
    require_present(result[0].simple_return, epsilon, 0.0, 1e-12);
    require_present(result[0].log_return, epsilon, 0.0, 1e-12);
}

TEST_CASE("ReturnSeries accepts positive subnormal equity when unchanged") {
    const double tiny = std::numeric_limits<double>::denorm_min();
    const auto result = build_return_series(tiny, make_snapshots({tiny}));
    require_present(result[0].simple_return, 0.0);
    require_present(result[0].log_return, 0.0);
}

TEST_CASE("ReturnSeries ignores cash and position details when equity is the same") {
    const std::vector<PortfolioSnapshot> snapshots{
        PortfolioSnapshot{session_date(0), 1000.0, 0, 0.0, 1000.0},
        PortfolioSnapshot{session_date(1), 600.0, 4, 500.0, 1100.0}
    };
    const auto result = build_return_series(1000.0, snapshots);
    REQUIRE(result.size() == 2);
    require_present(result[1].simple_return, 0.10);
    require_present(result[1].log_return, 0.095310179804325);
}

TEST_CASE("ReturnSeries does not modify its input snapshots") {
    const auto snapshots = make_snapshots({1000.0, 1100.0, 900.0});
    const auto original = snapshots;
    const auto result = build_return_series(1000.0, snapshots);
    REQUIRE(result.size() == original.size());
    REQUIRE(snapshots.size() == original.size());
    for (std::size_t i = 0; i < snapshots.size(); ++i) {
        REQUIRE(snapshots[i].session_date() == original[i].session_date());
        REQUIRE(snapshots[i].cash() == original[i].cash());
        REQUIRE(snapshots[i].position_quantity() == original[i].position_quantity());
        REQUIRE(snapshots[i].market_value() == original[i].market_value());
        REQUIRE(snapshots[i].equity() == original[i].equity());
    }
}

TEST_CASE("ReturnSeries rejects invalid later dates without mutating its inputs") {
    const std::vector<PortfolioSnapshot> snapshots{
        make_snapshot(0, 1000.0),
        make_snapshot(2, 1100.0),
        make_snapshot(1, 1200.0)
    };
    REQUIRE_THROWS_AS(
        build_return_series(1000.0, snapshots), std::invalid_argument
    );
    REQUIRE(snapshots.size() == 3);
    REQUIRE(snapshots[0].equity() == 1000.0);
    REQUIRE(snapshots[1].equity() == 1100.0);
    REQUIRE(snapshots[2].equity() == 1200.0);
}

TEST_CASE("ReturnSeries produces deterministic results in repeated calls") {
    const auto snapshots = make_snapshots({
        1000.0, 975.0, 1001.0, 989.0, 1040.0, 1075.0
    });
    const auto first = build_return_series(1000.0, snapshots);
    const auto second = build_return_series(1000.0, snapshots);
    REQUIRE(first.size() == second.size());
    for (std::size_t i = 0; i < first.size(); ++i) {
        require_same_observation(first[i], second[i]);
    }
}

TEST_CASE("ReturnSeries decisions never depend on future snapshots") {
    const auto snapshots = make_snapshots({
        1000.0, 1100.0, 1050.0, 1200.0, 900.0, 1150.0
    });
    const std::span<const PortfolioSnapshot> all{snapshots};
    const auto prefix = build_return_series(1000.0, all.first(3));
    const auto full = build_return_series(1000.0, all);
    REQUIRE(prefix.size() == 3);
    REQUIRE(full.size() == 6);
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        require_same_observation(full[i], prefix[i]);
    }
}

TEST_CASE("ReturnSeries supports explicit subranges without copying data") {
    const auto snapshots = make_snapshots({900.0, 1000.0, 1100.0, 990.0});
    const std::span<const PortfolioSnapshot> view{snapshots};
    const auto result = build_return_series(1000.0, view.subspan(1, 2));
    REQUIRE(result.size() == 2);
    REQUIRE(result[0].session_date == session_date(1));
    REQUIRE(result[1].session_date == session_date(2));
    require_present(result[0].simple_return, 0.0);
    require_present(result[1].simple_return, 0.10);
}

TEST_CASE("ReturnSeries scales linearly over a long deterministic history") {
    constexpr int count = 5000;
    std::vector<PortfolioSnapshot> snapshots;
    snapshots.reserve(count);
    for (int i = 0; i < count; ++i) {
        snapshots.push_back(make_snapshot(
            i, 1000.0 + static_cast<double>(i % 17)
        ));
    }
    const auto result = build_return_series(1000.0, snapshots);
    REQUIRE(result.size() == snapshots.size());
    REQUIRE(result.front().session_date == session_date(0));
    REQUIRE(result.back().session_date == session_date(count - 1));
    require_present(result.front().simple_return, 0.0);
    // i=4999 has remainder 1 when divided by 17; i=4998 remainder 0.
    require_present(result.back().simple_return, 1.0 / 1000.0);
    for (const auto& observation : result) {
        REQUIRE(observation.simple_return.has_value());
        REQUIRE(observation.log_return.has_value());
    }
}
