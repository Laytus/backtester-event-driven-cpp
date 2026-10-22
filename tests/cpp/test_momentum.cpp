#include "backtester/indicators/momentum.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

using backtester::indicators::Momentum;
using backtester::indicators::MomentumMode;

}  // namespace

TEST_CASE("Momentum validates its configuration and exposes parameters") {
    REQUIRE_THROWS_AS(Momentum(0), std::invalid_argument);

    const Momentum default_momentum;
    REQUIRE(default_momentum.period() == 10);
    REQUIRE(default_momentum.mode() == MomentumMode::Absolute);
    REQUIRE_FALSE(default_momentum.value().has_value());

    const Momentum custom_absolute{3};
    REQUIRE(custom_absolute.period() == 3);
    REQUIRE(custom_absolute.mode() == MomentumMode::Absolute);
    REQUIRE_FALSE(custom_absolute.value().has_value());

    const Momentum custom_percentage{2, MomentumMode::Percentage};
    REQUIRE(custom_percentage.period() == 2);
    REQUIRE(custom_percentage.mode() == MomentumMode::Percentage);
    REQUIRE_FALSE(custom_percentage.value().has_value());
}

TEST_CASE("Momentum rejects unrecognized modes") {
    REQUIRE_THROWS_AS(
        Momentum(3, static_cast<MomentumMode>(99)),
        std::invalid_argument
    );
}

TEST_CASE("Momentum default period first becomes ready at observation eleven") {
    Momentum momentum;

    for (int i = 0; i < 10; ++i) {
        momentum.update(100.0);
        REQUIRE_FALSE(momentum.value().has_value());
    }

    momentum.update(110.0);
    REQUIRE(momentum.value().has_value());
    REQUIRE(*momentum.value() == Catch::Approx(10.0));
}

TEST_CASE("Momentum absolute mode matches the approved oracle") {
    Momentum momentum{3, MomentumMode::Absolute};

    for (const double observation : {100.0, 110.0, 120.0}) {
        momentum.update(observation);
        REQUIRE_FALSE(momentum.value().has_value());
    }

    momentum.update(90.0);  // 90 - 100.
    REQUIRE(*momentum.value() == Catch::Approx(-10.0));

    momentum.update(121.0);  // 121 - 110.
    REQUIRE(*momentum.value() == Catch::Approx(11.0));

    momentum.update(100.0);  // 100 - 120.
    REQUIRE(*momentum.value() == Catch::Approx(-20.0));
}

TEST_CASE("Momentum percentage mode matches the approved oracle") {
    Momentum momentum{3, MomentumMode::Percentage};

    for (const double observation : {100.0, 110.0, 120.0}) {
        momentum.update(observation);
        REQUIRE_FALSE(momentum.value().has_value());
    }

    momentum.update(90.0);  // (90 - 100) / 100 * 100.
    REQUIRE(*momentum.value() == Catch::Approx(-10.0));

    momentum.update(121.0);  // (121 - 110) / 110 * 100.
    REQUIRE(*momentum.value() == Catch::Approx(10.0));

    momentum.update(100.0);  // (100 - 120) / 120 * 100.
    REQUIRE(*momentum.value() == Catch::Approx(-100.0 / 6.0));
}

TEST_CASE("Momentum rolling history advances by exactly one observation") {
    Momentum momentum{2};

    momentum.update(10.0);
    momentum.update(20.0);
    REQUIRE_FALSE(momentum.value().has_value());

    momentum.update(30.0);  // 30 - 10.
    REQUIRE(*momentum.value() == Catch::Approx(20.0));

    momentum.update(5.0);  // 5 - 20, not 5 - 10 or 5 - 30.
    REQUIRE(*momentum.value() == Catch::Approx(-15.0));

    momentum.update(8.0);  // 8 - 30.
    REQUIRE(*momentum.value() == Catch::Approx(-22.0));
}

TEST_CASE("Momentum period one compares each price with the preceding price") {
    Momentum absolute{1, MomentumMode::Absolute};
    Momentum percentage{1, MomentumMode::Percentage};

    absolute.update(100.0);
    percentage.update(100.0);
    REQUIRE_FALSE(absolute.value().has_value());
    REQUIRE_FALSE(percentage.value().has_value());

    absolute.update(105.0);
    percentage.update(105.0);
    REQUIRE(*absolute.value() == Catch::Approx(5.0));
    REQUIRE(*percentage.value() == Catch::Approx(5.0));

    absolute.update(103.0);
    percentage.update(103.0);
    REQUIRE(*absolute.value() == Catch::Approx(-2.0));
    REQUIRE(*percentage.value() == Catch::Approx((-2.0 / 105.0) * 100.0));
}

TEST_CASE("Momentum absolute mode accepts finite zero and negative prices") {
    Momentum momentum{2};

    momentum.update(-5.0);
    momentum.update(0.0);
    REQUIRE_FALSE(momentum.value().has_value());

    momentum.update(-2.0);  // -2 - (-5).
    REQUIRE(*momentum.value() == Catch::Approx(3.0));

    momentum.update(-7.0);  // -7 - 0.
    REQUIRE(*momentum.value() == Catch::Approx(-7.0));

    momentum.update(-2.0);  // -2 - (-2).
    REQUIRE(*momentum.value() == Catch::Approx(0.0).margin(1e-12));
}

TEST_CASE("Momentum percentage mode rejects non-positive inputs during warm-up") {
    Momentum momentum{2, MomentumMode::Percentage};
    momentum.update(100.0);

    for (const double invalid : {0.0, -0.0, -1.0}) {
        REQUIRE_THROWS_AS(momentum.update(invalid), std::invalid_argument);
        REQUIRE_FALSE(momentum.value().has_value());
    }

    momentum.update(110.0);
    REQUIRE_FALSE(momentum.value().has_value());

    momentum.update(121.0);
    REQUIRE(*momentum.value() == Catch::Approx(21.0));
}

TEST_CASE("Momentum percentage mode rejects non-positive inputs when ready") {
    Momentum momentum{2, MomentumMode::Percentage};
    momentum.update(100.0);
    momentum.update(110.0);
    momentum.update(120.0);
    REQUIRE(*momentum.value() == Catch::Approx(20.0));

    for (const double invalid : {0.0, -0.0, -2.0}) {
        REQUIRE_THROWS_AS(momentum.update(invalid), std::invalid_argument);
        REQUIRE(*momentum.value() == Catch::Approx(20.0));
    }

    momentum.update(121.0);  // Reference remains 110, not a rejected price.
    REQUIRE(*momentum.value() == Catch::Approx(10.0));
}

TEST_CASE("Momentum rejects non-finite inputs without advancing warm-up") {
    Momentum momentum{2};
    momentum.update(10.0);

    const std::vector<double> invalid_inputs{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    for (const double invalid : invalid_inputs) {
        REQUIRE_THROWS_AS(momentum.update(invalid), std::invalid_argument);
        REQUIRE_FALSE(momentum.value().has_value());
    }

    momentum.update(20.0);
    REQUIRE_FALSE(momentum.value().has_value());

    momentum.update(30.0);
    REQUIRE(*momentum.value() == Catch::Approx(20.0));
}

TEST_CASE("Momentum rejects non-finite inputs without changing a ready value") {
    Momentum momentum{2, MomentumMode::Percentage};
    momentum.update(100.0);
    momentum.update(110.0);
    momentum.update(120.0);
    REQUIRE(*momentum.value() == Catch::Approx(20.0));

    const std::vector<double> invalid_inputs{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    for (const double invalid : invalid_inputs) {
        REQUIRE_THROWS_AS(momentum.update(invalid), std::invalid_argument);
        REQUIRE(*momentum.value() == Catch::Approx(20.0));
    }

    momentum.update(121.0);
    REQUIRE(*momentum.value() == Catch::Approx(10.0));
}

TEST_CASE("Momentum value queries do not change the indicator state") {
    Momentum momentum{2, MomentumMode::Absolute};
    momentum.update(10.0);
    momentum.update(20.0);
    momentum.update(30.0);

    const auto first = momentum.value();
    const auto second = momentum.value();

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(*first == Catch::Approx(20.0));
    REQUIRE(*second == *first);
    REQUIRE(momentum.period() == 2);
    REQUIRE(momentum.mode() == MomentumMode::Absolute);

    momentum.update(35.0);  // 35 - 20.
    REQUIRE(*momentum.value() == Catch::Approx(15.0));
}

TEST_CASE("Momentum absolute mode preserves state after difference overflow") {
    Momentum momentum{1, MomentumMode::Absolute};
    const double max_value = std::numeric_limits<double>::max();

    momentum.update(max_value);
    REQUIRE_FALSE(momentum.value().has_value());

    momentum.update(max_value);
    REQUIRE(*momentum.value() == Catch::Approx(0.0).margin(1e-12));

    REQUIRE_THROWS_AS(momentum.update(-max_value), std::overflow_error);
    REQUIRE(*momentum.value() == Catch::Approx(0.0).margin(1e-12));

    // The rejected price must not replace the reference in history.
    momentum.update(max_value / 2.0);
    REQUIRE(*momentum.value() == Catch::Approx(-max_value / 2.0));
}

TEST_CASE("Momentum percentage mode preserves state after result overflow") {
    Momentum momentum{1, MomentumMode::Percentage};

    momentum.update(1e-300);
    REQUIRE_FALSE(momentum.value().has_value());

    momentum.update(1e-300);
    REQUIRE(*momentum.value() == Catch::Approx(0.0).margin(1e-12));

    // The difference is finite, but division by the reference overflows.
    REQUIRE_THROWS_AS(momentum.update(1e300), std::overflow_error);
    REQUIRE(*momentum.value() == Catch::Approx(0.0).margin(1e-12));

    momentum.update(2e-300);  // (2e-300 - 1e-300) / 1e-300 = 100%.
    REQUIRE(*momentum.value() == Catch::Approx(100.0));
}

TEST_CASE("Momentum percentage mode preserves tiny representable changes") {
    Momentum momentum{1, MomentumMode::Percentage};
    const double reference = 100.0;
    const double next = std::nextafter(
        reference,
        std::numeric_limits<double>::infinity()
    );

    momentum.update(reference);
    momentum.update(next);

    REQUIRE(momentum.value().has_value());
    REQUIRE(*momentum.value() > 0.0);
    // With a reference of 100, the numerical percent equals the price delta.
    REQUIRE(*momentum.value() == Catch::Approx(next - reference).epsilon(1e-12));
}

TEST_CASE("Momentum handles large finite prices without spurious overflow") {
    Momentum absolute{1, MomentumMode::Absolute};
    Momentum percentage{1, MomentumMode::Percentage};

    absolute.update(1e300);
    percentage.update(1e300);

    absolute.update(1.01e300);
    percentage.update(1.01e300);

    REQUIRE(absolute.value().has_value());
    REQUIRE(percentage.value().has_value());
    REQUIRE(std::isfinite(*absolute.value()));
    REQUIRE(std::isfinite(*percentage.value()));
    REQUIRE(*absolute.value() == Catch::Approx(1e298).epsilon(1e-12));
    REQUIRE(*percentage.value() == Catch::Approx(1.0).epsilon(1e-12));
}

TEST_CASE("Momentum is deterministic for identical sequences and configurations") {
    const std::vector<double> prices{100.0, 110.0, 120.0, 90.0, 121.0, 100.0};

    for (const MomentumMode mode : {
             MomentumMode::Absolute,
             MomentumMode::Percentage
         }) {
        Momentum first{3, mode};
        Momentum second{3, mode};

        for (const double observation : prices) {
            first.update(observation);
            second.update(observation);
            REQUIRE(first.value() == second.value());
        }
    }
}
