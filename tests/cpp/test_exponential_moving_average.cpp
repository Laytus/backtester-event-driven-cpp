#include "backtester/indicators/exponential_moving_average.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

using backtester::indicators::ExponentialMovingAverage;

}  // namespace

TEST_CASE("EMA validates its configuration and exposes effective parameters") {
    REQUIRE_THROWS_AS(ExponentialMovingAverage(0), std::invalid_argument);

    const ExponentialMovingAverage default_ema;
    REQUIRE(default_ema.period() == 20);
    REQUIRE(default_ema.alpha() == Catch::Approx(2.0 / 21.0));
    REQUIRE_FALSE(default_ema.value().has_value());

    const ExponentialMovingAverage custom_period{10};
    REQUIRE(custom_period.period() == 10);
    REQUIRE(custom_period.alpha() == Catch::Approx(2.0 / 11.0));

    const ExponentialMovingAverage custom_alpha{10, 0.3};
    REQUIRE(custom_alpha.period() == 10);
    REQUIRE(custom_alpha.alpha() == Catch::Approx(0.3));
    REQUIRE_FALSE(custom_alpha.value().has_value());
}

TEST_CASE("EMA rejects invalid smoothing factors") {
    REQUIRE_THROWS_AS(ExponentialMovingAverage(3, 0.0), std::invalid_argument);
    REQUIRE_THROWS_AS(ExponentialMovingAverage(3, -0.1), std::invalid_argument);
    REQUIRE_THROWS_AS(ExponentialMovingAverage(3, 1.01), std::invalid_argument);
    REQUIRE_THROWS_AS(
        ExponentialMovingAverage(3, std::numeric_limits<double>::quiet_NaN()),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        ExponentialMovingAverage(3, std::numeric_limits<double>::infinity()),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        ExponentialMovingAverage(3, -std::numeric_limits<double>::infinity()),
        std::invalid_argument
    );

    const ExponentialMovingAverage upper_bound{3, 1.0};
    REQUIRE(upper_bound.alpha() == Catch::Approx(1.0));
}

TEST_CASE("EMA default period first becomes ready at observation twenty") {
    ExponentialMovingAverage ema;

    for (int i = 0; i < 19; ++i) {
        ema.update(100.0);
        REQUIRE_FALSE(ema.value().has_value());
    }

    ema.update(100.0);
    REQUIRE(ema.value().has_value());
    REQUIRE(*ema.value() == Catch::Approx(100.0));

    ema.update(121.0);
    REQUIRE(*ema.value() == Catch::Approx(102.0));  // alpha = 2/21.
}

TEST_CASE("EMA matches the approved SMA-seeded oracle") {
    ExponentialMovingAverage ema{3};  // Default alpha = 0.5.

    ema.update(100.0);
    REQUIRE_FALSE(ema.value().has_value());

    ema.update(102.0);
    REQUIRE_FALSE(ema.value().has_value());

    ema.update(104.0);
    REQUIRE(ema.value().has_value());
    REQUIRE(*ema.value() == Catch::Approx(102.0));

    ema.update(103.0);
    REQUIRE(*ema.value() == Catch::Approx(102.5));

    ema.update(107.0);
    REQUIRE(*ema.value() == Catch::Approx(104.75));
}

TEST_CASE("EMA custom alpha changes the recurrence but not the SMA seed") {
    ExponentialMovingAverage ema{3, 0.25};

    ema.update(100.0);
    ema.update(110.0);
    REQUIRE_FALSE(ema.value().has_value());

    ema.update(120.0);
    REQUIRE(*ema.value() == Catch::Approx(110.0));

    ema.update(130.0);
    REQUIRE(*ema.value() == Catch::Approx(115.0));

    ema.update(90.0);
    REQUIRE(*ema.value() == Catch::Approx(108.75));
}

TEST_CASE("EMA alpha one uses the SMA seed then follows every observation") {
    ExponentialMovingAverage ema{3, 1.0};

    ema.update(100.0);
    ema.update(102.0);
    REQUIRE_FALSE(ema.value().has_value());

    ema.update(104.0);
    REQUIRE(*ema.value() == Catch::Approx(102.0));

    ema.update(50.0);
    REQUIRE(*ema.value() == Catch::Approx(50.0));

    ema.update(-12.0);
    REQUIRE(*ema.value() == Catch::Approx(-12.0));
}

TEST_CASE("EMA period one with default alpha follows each observation") {
    ExponentialMovingAverage ema{1};
    REQUIRE(ema.alpha() == Catch::Approx(1.0));
    REQUIRE_FALSE(ema.value().has_value());

    for (const double price : {4.0, 0.0, -3.5, 12.25}) {
        ema.update(price);
        REQUIRE(ema.value().has_value());
        REQUIRE(*ema.value() == Catch::Approx(price));
    }
}

TEST_CASE("EMA period one with custom alpha still smooths later observations") {
    ExponentialMovingAverage ema{1, 0.5};

    ema.update(100.0);
    REQUIRE(*ema.value() == Catch::Approx(100.0));

    ema.update(120.0);
    REQUIRE(*ema.value() == Catch::Approx(110.0));

    ema.update(90.0);
    REQUIRE(*ema.value() == Catch::Approx(100.0));
}

TEST_CASE("EMA accepts zero and negative finite observations") {
    ExponentialMovingAverage ema{3};

    ema.update(-6.0);
    ema.update(0.0);
    ema.update(3.0);
    REQUIRE(*ema.value() == Catch::Approx(-1.0));

    ema.update(9.0);
    REQUIRE(*ema.value() == Catch::Approx(4.0));

    ema.update(-8.0);
    REQUIRE(*ema.value() == Catch::Approx(-2.0));
}

TEST_CASE("EMA value queries do not modify indicator state") {
    ExponentialMovingAverage ema{2};  // Alpha = 2/3.
    ema.update(10.0);
    ema.update(14.0);

    const auto first = ema.value();
    const auto second = ema.value();

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(*first == Catch::Approx(12.0));
    REQUIRE(*second == *first);
    REQUIRE(ema.period() == 2);
    REQUIRE(ema.alpha() == Catch::Approx(2.0 / 3.0));

    ema.update(18.0);
    REQUIRE(*ema.value() == Catch::Approx(16.0));
}

TEST_CASE("EMA rejects non-finite observations without advancing warm-up") {
    ExponentialMovingAverage ema{3};
    ema.update(100.0);

    const std::vector<double> invalid_inputs{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    for (const double invalid : invalid_inputs) {
        REQUIRE_THROWS_AS(ema.update(invalid), std::invalid_argument);
        REQUIRE_FALSE(ema.value().has_value());
    }

    ema.update(102.0);
    REQUIRE_FALSE(ema.value().has_value());

    ema.update(104.0);
    REQUIRE(*ema.value() == Catch::Approx(102.0));
}

TEST_CASE("EMA rejects non-finite observations without changing a ready value") {
    ExponentialMovingAverage ema{2};
    ema.update(10.0);
    ema.update(14.0);
    REQUIRE(*ema.value() == Catch::Approx(12.0));

    const std::vector<double> invalid_inputs{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    for (const double invalid : invalid_inputs) {
        REQUIRE_THROWS_AS(ema.update(invalid), std::invalid_argument);
        REQUIRE(*ema.value() == Catch::Approx(12.0));
    }

    ema.update(18.0);
    REQUIRE(*ema.value() == Catch::Approx(16.0));
}

TEST_CASE("EMA handles large finite observations without intermediate sum overflow") {
    ExponentialMovingAverage ema{2};
    const double max_value = std::numeric_limits<double>::max();

    ema.update(max_value);
    REQUIRE_FALSE(ema.value().has_value());

    ema.update(max_value);
    REQUIRE(ema.value().has_value());
    REQUIRE(std::isfinite(*ema.value()));
    REQUIRE(*ema.value() == max_value);

    ema.update(-max_value);
    REQUIRE(std::isfinite(*ema.value()));
    REQUIRE(*ema.value() == Catch::Approx(-max_value / 3.0).epsilon(1e-12));
}

TEST_CASE("EMA is deterministic for identical observations and configurations") {
    ExponentialMovingAverage first{3, 0.25};
    ExponentialMovingAverage second{3, 0.25};

    for (const double observation : {100.0, 102.0, 104.0, 103.0, 107.0, -5.0}) {
        first.update(observation);
        second.update(observation);
        REQUIRE(first.value() == second.value());
        REQUIRE(first.period() == second.period());
        REQUIRE(first.alpha() == second.alpha());
    }
}
