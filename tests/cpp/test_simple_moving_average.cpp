#include "backtester/indicators/simple_moving_average.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <stdexcept>
#include <vector>

namespace {

using backtester::indicators::SimpleMovingAverage;

}  // namespace

TEST_CASE("SMA validates its period and exposes the configured period") {
    REQUIRE_THROWS_AS(SimpleMovingAverage{0}, std::invalid_argument);

    const SimpleMovingAverage default_sma;
    REQUIRE(default_sma.period() == 20);
    REQUIRE_FALSE(default_sma.value().has_value());

    const SimpleMovingAverage custom_sma{3};
    REQUIRE(custom_sma.period() == 3);
    REQUIRE_FALSE(custom_sma.value().has_value());
}

TEST_CASE("SMA default period first becomes ready at observation twenty") {
    SimpleMovingAverage sma;

    for (int i = 0; i < 19; ++i) {
        sma.update(100.0);
        REQUIRE_FALSE(sma.value().has_value());
    }

    sma.update(100.0);
    REQUIRE(sma.value().has_value());
    REQUIRE(*sma.value() == Catch::Approx(100.0));
}

TEST_CASE("SMA matches the approved rolling-window oracle") {
    SimpleMovingAverage sma{3};

    sma.update(100.0);
    REQUIRE_FALSE(sma.value().has_value());

    sma.update(102.0);
    REQUIRE_FALSE(sma.value().has_value());

    sma.update(104.0);
    REQUIRE(sma.value().has_value());
    REQUIRE(*sma.value() == Catch::Approx(102.0));

    sma.update(103.0);
    REQUIRE(*sma.value() == Catch::Approx(103.0));

    sma.update(107.0);
    REQUIRE(*sma.value() == Catch::Approx(314.0 / 3.0));
}

TEST_CASE("SMA with period one tracks each new observation") {
    SimpleMovingAverage sma{1};

    REQUIRE_FALSE(sma.value().has_value());

    for (const double observation : {4.0, 0.0, -3.5, 12.25}) {
        sma.update(observation);
        REQUIRE(sma.value().has_value());
        REQUIRE(*sma.value() == Catch::Approx(observation));
    }
}

TEST_CASE("SMA accepts finite zero and negative observations") {
    SimpleMovingAverage sma{3};

    sma.update(-6.0);
    sma.update(0.0);
    sma.update(3.0);
    REQUIRE(*sma.value() == Catch::Approx(-1.0));

    sma.update(9.0);  // Window: [0, 3, 9].
    REQUIRE(*sma.value() == Catch::Approx(4.0));

    sma.update(-12.0);  // Window: [3, 9, -12].
    REQUIRE(*sma.value() == Catch::Approx(0.0).margin(1e-12));
}

TEST_CASE("SMA value queries do not change the indicator state") {
    SimpleMovingAverage sma{2};
    sma.update(10.0);
    sma.update(14.0);

    const auto first = sma.value();
    const auto second = sma.value();

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(*first == Catch::Approx(12.0));
    REQUIRE(*second == *first);
    REQUIRE(sma.period() == 2);

    sma.update(18.0);
    REQUIRE(*sma.value() == Catch::Approx(16.0));
}

TEST_CASE("SMA rejects invalid input without advancing warm-up") {
    SimpleMovingAverage sma{2};
    sma.update(10.0);

    REQUIRE_THROWS_AS(
        sma.update(std::numeric_limits<double>::quiet_NaN()),
        std::invalid_argument
    );
    REQUIRE_FALSE(sma.value().has_value());

    sma.update(20.0);
    REQUIRE(*sma.value() == Catch::Approx(15.0));
}

TEST_CASE("SMA rejects non-finite inputs without changing a ready value") {
    SimpleMovingAverage sma{2};
    sma.update(10.0);
    sma.update(20.0);

    const std::vector<double> invalid_observations{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    for (const double invalid : invalid_observations) {
        REQUIRE_THROWS_AS(sma.update(invalid), std::invalid_argument);
        REQUIRE(sma.value().has_value());
        REQUIRE(*sma.value() == Catch::Approx(15.0));
    }

    sma.update(30.0);  // Window must be [20, 30].
    REQUIRE(*sma.value() == Catch::Approx(25.0));
}

TEST_CASE("SMA preserves state after an arithmetic overflow") {
    SimpleMovingAverage sma{2};
    const double max_value = std::numeric_limits<double>::max();

    sma.update(max_value);
    REQUIRE_FALSE(sma.value().has_value());

    REQUIRE_THROWS_AS(sma.update(max_value), std::overflow_error);
    REQUIRE_FALSE(sma.value().has_value());

    // The rejected value must not enter the window.
    sma.update(-max_value);
    REQUIRE(sma.value().has_value());
    REQUIRE(*sma.value() == Catch::Approx(0.0).margin(1e-12));
}

TEST_CASE("SMA Neumaier summation retains small terms under cancellation") {
    SimpleMovingAverage sma{3};

    sma.update(1e16);
    sma.update(1.0);
    sma.update(-1e16);

    REQUIRE(sma.value().has_value());
    REQUIRE(*sma.value() == Catch::Approx(1.0 / 3.0).margin(1e-12));

    // Check that compensation remains correct as the window advances.
    sma.update(1e16);  // Window: [1, -1e16, 1e16].
    REQUIRE(*sma.value() == Catch::Approx(1.0 / 3.0).margin(1e-12));

    sma.update(2.0);  // Window: [-1e16, 1e16, 2].
    REQUIRE(*sma.value() == Catch::Approx(2.0 / 3.0).margin(1e-12));
}

TEST_CASE("SMA is deterministic for identical input sequences") {
    SimpleMovingAverage first{3};
    SimpleMovingAverage second{3};

    for (const double observation : {100.0, 102.0, 104.0, 103.0, 107.0}) {
        first.update(observation);
        second.update(observation);
        REQUIRE(first.value() == second.value());
    }
}
