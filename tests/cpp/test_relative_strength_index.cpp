#include "backtester/indicators/relative_strength_index.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

using backtester::indicators::RelativeStrengthIndex;
using backtester::indicators::RSIMode;

void require_unavailable(const RelativeStrengthIndex& rsi) {
    REQUIRE_FALSE(rsi.value().has_value());
}

void require_rsi(const RelativeStrengthIndex& rsi, double expected) {
    const auto actual = rsi.value();
    REQUIRE(actual.has_value());
    REQUIRE(std::isfinite(*actual));
    REQUIRE(*actual >= 0.0);
    REQUIRE(*actual <= 100.0);
    REQUIRE(*actual == Catch::Approx(expected).margin(1e-10));
}

}  // namespace

TEST_CASE("RSI exposes default and custom configuration") {
    const RelativeStrengthIndex default_rsi;
    REQUIRE(default_rsi.period() == 14);
    REQUIRE(default_rsi.mode() == RSIMode::Directional);
    REQUIRE(default_rsi.beta() == Catch::Approx(2.0 / 15.0));
    require_unavailable(default_rsi);

    const RelativeStrengthIndex directional{5};
    REQUIRE(directional.period() == 5);
    REQUIRE(directional.mode() == RSIMode::Directional);
    REQUIRE(directional.beta() == Catch::Approx(1.0 / 3.0));

    const RelativeStrengthIndex magnitude{10, RSIMode::Magnitude};
    REQUIRE(magnitude.period() == 10);
    REQUIRE(magnitude.mode() == RSIMode::Magnitude);
    REQUIRE(magnitude.beta() == Catch::Approx(0.1));

    const RelativeStrengthIndex custom{3, RSIMode::Magnitude, 0.4};
    REQUIRE(custom.period() == 3);
    REQUIRE(custom.mode() == RSIMode::Magnitude);
    REQUIRE(custom.beta() == Catch::Approx(0.4));
    require_unavailable(custom);
}

TEST_CASE("RSI rejects zero periods and unrecognized modes") {
    REQUIRE_THROWS_AS(RelativeStrengthIndex(0), std::invalid_argument);
    REQUIRE_THROWS_AS(
        RelativeStrengthIndex(0, RSIMode::Magnitude),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        RelativeStrengthIndex(3, static_cast<RSIMode>(99)),
        std::invalid_argument
    );
}

TEST_CASE("RSI rejects invalid beta overrides in both modes") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    for (const auto mode : {RSIMode::Directional, RSIMode::Magnitude}) {
        for (const double beta : {0.0, -0.1, 1.01, nan, inf, -inf}) {
            REQUIRE_THROWS_AS(
                RelativeStrengthIndex(3, mode, beta),
                std::invalid_argument
            );
        }
        const RelativeStrengthIndex valid{3, mode, 1.0};
        REQUIRE(valid.beta() == 1.0);
    }
}

TEST_CASE("RSI default period first becomes ready at price observation fifteen") {
    for (const auto mode : {RSIMode::Directional, RSIMode::Magnitude}) {
        RelativeStrengthIndex rsi{14, mode};
        for (int i = 0; i < 14; ++i) {
            rsi.update(100.0 + static_cast<double>(i));
            require_unavailable(rsi);
        }
        rsi.update(114.0);  // Fourteen positive changes after fifteen prices.
        require_rsi(rsi, 100.0);
    }
}

TEST_CASE("RSI directional mode matches the approved six-price oracle") {
    RelativeStrengthIndex rsi{3, RSIMode::Directional, 0.5};

    for (const double price : {100.0, 104.0, 102.0}) {
        rsi.update(price);
        require_unavailable(rsi);
    }

    rsi.update(103.0);  // Seed: up = 2/3, down = 1/3.
    require_rsi(rsi, 200.0 / 3.0);

    rsi.update(103.0);  // Flat price scales both averages equally.
    require_rsi(rsi, 200.0 / 3.0);

    rsi.update(101.0);  // Up = 1/6, down = 7/12.
    require_rsi(rsi, 200.0 / 9.0);
}

TEST_CASE("RSI magnitude mode matches the approved six-price oracle") {
    RelativeStrengthIndex rsi{3, RSIMode::Magnitude, 0.5};

    for (const double price : {100.0, 104.0, 102.0}) {
        rsi.update(price);
        require_unavailable(rsi);
    }

    rsi.update(103.0);  // Seed: average gain = 5/3, loss = 2/3.
    require_rsi(rsi, 500.0 / 7.0);

    rsi.update(103.0);  // Both averages are multiplied by 1/2.
    require_rsi(rsi, 500.0 / 7.0);

    rsi.update(101.0);  // Gain = 5/12, loss = 7/6.
    require_rsi(rsi, 500.0 / 19.0);
}

TEST_CASE("RSI directional mode ignores price change magnitude") {
    RelativeStrengthIndex small{2, RSIMode::Directional};
    RelativeStrengthIndex large{2, RSIMode::Directional};
    RelativeStrengthIndex magnitude{2, RSIMode::Magnitude};

    for (const double price : {100.0, 102.0, 101.0}) {
        small.update(price);
    }
    for (const double price : {100.0, 200.0, 199.0}) {
        large.update(price);
        magnitude.update(price);
    }

    require_rsi(small, 50.0);
    require_rsi(large, 50.0);
    require_rsi(magnitude, 10000.0 / 101.0);
}

TEST_CASE("RSI default smoothing factors match independent recurrence oracles") {
    RelativeStrengthIndex directional{2, RSIMode::Directional};
    RelativeStrengthIndex magnitude{2, RSIMode::Magnitude};

    for (const double price : {100.0, 102.0, 101.0}) {
        directional.update(price);
        magnitude.update(price);
    }
    require_rsi(directional, 50.0);
    require_rsi(magnitude, 200.0 / 3.0);

    directional.update(103.0);  // beta = 2/3; up = 5/6, down = 1/6.
    magnitude.update(103.0);    // beta = 1/2; gain = 3/2, loss = 1/4.
    require_rsi(directional, 250.0 / 3.0);
    require_rsi(magnitude, 600.0 / 7.0);
}

TEST_CASE("RSI custom beta changes smoothing but not the initial seed") {
    RelativeStrengthIndex directional{2, RSIMode::Directional, 0.25};
    RelativeStrengthIndex magnitude{2, RSIMode::Magnitude, 0.25};

    for (const double price : {100.0, 102.0, 101.0}) {
        directional.update(price);
        magnitude.update(price);
    }
    require_rsi(directional, 50.0);
    require_rsi(magnitude, 200.0 / 3.0);

    directional.update(103.0);  // Up = 5/8, down = 3/8.
    magnitude.update(103.0);    // Gain = 5/4, loss = 3/8.
    require_rsi(directional, 62.5);
    require_rsi(magnitude, 1000.0 / 13.0);
}

TEST_CASE("RSI beta one applies only after seeding and handles flat changes") {
    RelativeStrengthIndex directional{2, RSIMode::Directional, 1.0};
    RelativeStrengthIndex magnitude{2, RSIMode::Magnitude, 1.0};

    for (const double price : {100.0, 102.0, 101.0}) {
        directional.update(price);
        magnitude.update(price);
    }
    require_rsi(directional, 50.0);
    require_rsi(magnitude, 200.0 / 3.0);

    for (auto* rsi : {&directional, &magnitude}) {
        rsi->update(103.0);
        require_rsi(*rsi, 100.0);

        rsi->update(103.0);
        require_rsi(*rsi, 50.0);

        rsi->update(101.0);
        require_rsi(*rsi, 0.0);
    }
}

TEST_CASE("RSI period one first becomes ready on the second price") {
    RelativeStrengthIndex directional{1, RSIMode::Directional};
    RelativeStrengthIndex magnitude{1, RSIMode::Magnitude};
    RelativeStrengthIndex custom_directional{1, RSIMode::Directional, 0.25};
    RelativeStrengthIndex custom_magnitude{1, RSIMode::Magnitude, 0.25};

    for (auto* rsi : {&directional, &magnitude, &custom_directional,
                      &custom_magnitude}) {
        rsi->update(100.0);
        require_unavailable(*rsi);
        rsi->update(110.0);
        require_rsi(*rsi, 100.0);
    }

    directional.update(105.0);
    magnitude.update(105.0);
    custom_directional.update(105.0);
    custom_magnitude.update(105.0);

    require_rsi(directional, 0.0);
    require_rsi(magnitude, 0.0);
    require_rsi(custom_directional, 75.0);
    require_rsi(custom_magnitude, 600.0 / 7.0);
}

TEST_CASE("RSI returns fifty for constant prices and remains neutral") {
    for (const auto mode : {RSIMode::Directional, RSIMode::Magnitude}) {
        RelativeStrengthIndex rsi{3, mode};
        for (int i = 0; i < 3; ++i) {
            rsi.update(42.0);
            require_unavailable(rsi);
        }
        for (int i = 0; i < 5; ++i) {
            rsi.update(42.0);
            require_rsi(rsi, 50.0);
        }
    }
}

TEST_CASE("RSI returns one hundred for exclusively upward movement") {
    for (const auto mode : {RSIMode::Directional, RSIMode::Magnitude}) {
        RelativeStrengthIndex rsi{2, mode};
        for (const double price : {100.0, 102.0, 105.0, 105.0}) {
            rsi.update(price);
        }
        require_rsi(rsi, 100.0);
    }
}

TEST_CASE("RSI returns zero for exclusively downward movement") {
    for (const auto mode : {RSIMode::Directional, RSIMode::Magnitude}) {
        RelativeStrengthIndex rsi{2, mode};
        for (const double price : {100.0, 98.0, 95.0, 95.0}) {
            rsi.update(price);
        }
        require_rsi(rsi, 0.0);
    }
}

TEST_CASE("RSI accepts zero and negative finite prices in both modes") {
    RelativeStrengthIndex directional{2, RSIMode::Directional};
    RelativeStrengthIndex magnitude{2, RSIMode::Magnitude};

    for (const double price : {-10.0, -8.0, -9.0}) {
        directional.update(price);
        magnitude.update(price);
    }
    require_rsi(directional, 50.0);
    require_rsi(magnitude, 200.0 / 3.0);

    directional.update(-7.0);
    magnitude.update(-7.0);
    require_rsi(directional, 250.0 / 3.0);
    require_rsi(magnitude, 600.0 / 7.0);

    directional.update(0.0);
    magnitude.update(0.0);
    REQUIRE(directional.value().has_value());
    REQUIRE(magnitude.value().has_value());
}

TEST_CASE("RSI value and parameter queries do not modify its state") {
    RelativeStrengthIndex rsi{2, RSIMode::Magnitude, 0.25};
    for (const double price : {100.0, 102.0, 101.0}) {
        rsi.update(price);
    }

    const auto first = rsi.value();
    const auto second = rsi.value();
    REQUIRE(first.has_value());
    REQUIRE(second == first);
    REQUIRE(rsi.period() == 2);
    REQUIRE(rsi.mode() == RSIMode::Magnitude);
    REQUIRE(rsi.beta() == Catch::Approx(0.25));

    rsi.update(103.0);
    require_rsi(rsi, 1000.0 / 13.0);
}

TEST_CASE("RSI rejects non-finite observations before establishing a reference") {
    const std::array<double, 3> invalid{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    for (const auto mode : {RSIMode::Directional, RSIMode::Magnitude}) {
        RelativeStrengthIndex rsi{1, mode};
        for (const double price : invalid) {
            REQUIRE_THROWS_AS(rsi.update(price), std::invalid_argument);
            require_unavailable(rsi);
        }
        rsi.update(100.0);
        require_unavailable(rsi);
        rsi.update(110.0);
        require_rsi(rsi, 100.0);
    }
}

TEST_CASE("RSI rejects non-finite observations without advancing warm-up") {
    const std::array<double, 3> invalid{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    for (const auto mode : {RSIMode::Directional, RSIMode::Magnitude}) {
        RelativeStrengthIndex rsi{3, mode, 0.5};
        rsi.update(100.0);
        rsi.update(104.0);
        require_unavailable(rsi);

        for (const double price : invalid) {
            REQUIRE_THROWS_AS(rsi.update(price), std::invalid_argument);
            require_unavailable(rsi);
        }

        rsi.update(102.0);
        require_unavailable(rsi);
        rsi.update(103.0);
        require_rsi(rsi, mode == RSIMode::Directional
            ? 200.0 / 3.0 : 500.0 / 7.0);
    }
}

TEST_CASE("RSI rejects non-finite observations without changing a ready state") {
    const std::array<double, 3> invalid{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    for (const auto mode : {RSIMode::Directional, RSIMode::Magnitude}) {
        RelativeStrengthIndex rsi{2, mode};
        RelativeStrengthIndex baseline{2, mode};
        for (const double price : {100.0, 102.0, 101.0}) {
            rsi.update(price);
            baseline.update(price);
        }

        const auto expected_before = baseline.value();
        REQUIRE(expected_before.has_value());
        for (const double price : invalid) {
            REQUIRE_THROWS_AS(rsi.update(price), std::invalid_argument);
            REQUIRE(rsi.value() == expected_before);
        }

        rsi.update(103.0);
        baseline.update(103.0);
        REQUIRE(rsi.value() == baseline.value());
    }
}

TEST_CASE("RSI directional mode compares extreme finite prices without overflow") {
    const double max = std::numeric_limits<double>::max();
    RelativeStrengthIndex rsi{1, RSIMode::Directional};

    rsi.update(-max);
    require_unavailable(rsi);
    rsi.update(max);  // Subtraction would overflow, comparison does not.
    require_rsi(rsi, 100.0);
    rsi.update(-max);
    require_rsi(rsi, 0.0);
}

TEST_CASE("RSI magnitude mode preserves reference after warm-up difference overflow") {
    const double max = std::numeric_limits<double>::max();
    RelativeStrengthIndex rsi{2, RSIMode::Magnitude};

    rsi.update(max);
    require_unavailable(rsi);
    REQUIRE_THROWS_AS(rsi.update(-max), std::overflow_error);
    require_unavailable(rsi);

    rsi.update(max);  // Valid first change = 0, not +2*max.
    require_unavailable(rsi);
    rsi.update(max / 2.0);  // Valid second change is negative.
    require_rsi(rsi, 0.0);
}

TEST_CASE("RSI magnitude mode preserves ready state after difference overflow") {
    const double max = std::numeric_limits<double>::max();
    RelativeStrengthIndex rsi{2, RSIMode::Magnitude};

    for (int i = 0; i < 3; ++i) {
        rsi.update(max);
    }
    require_rsi(rsi, 50.0);

    REQUIRE_THROWS_AS(rsi.update(-max), std::overflow_error);
    require_rsi(rsi, 50.0);

    rsi.update(max / 2.0);
    require_rsi(rsi, 0.0);
}

TEST_CASE("RSI normalizes large finite magnitude changes without sum overflow") {
    const double max = std::numeric_limits<double>::max();
    RelativeStrengthIndex rsi{2, RSIMode::Magnitude};

    rsi.update(0.0);
    rsi.update(max);
    require_unavailable(rsi);
    rsi.update(0.0);  // Seed: average gain = loss = max/2.
    require_rsi(rsi, 50.0);

    rsi.update(max);  // Gain = 3*max/4, loss = max/4.
    require_rsi(rsi, 75.0);
}

TEST_CASE("RSI is deterministic for identical inputs in both configurations") {
    const std::vector<double> prices{
        100.0, 104.0, 102.0, 103.0, 103.0, 101.0,
        105.0, 100.0, 107.0, 106.0, 108.0
    };

    for (const auto mode : {RSIMode::Directional, RSIMode::Magnitude}) {
        RelativeStrengthIndex lhs{3, mode, 0.4};
        RelativeStrengthIndex rhs{3, mode, 0.4};

        for (const double price : prices) {
            lhs.update(price);
            rhs.update(price);
            REQUIRE(lhs.value() == rhs.value());
            if (lhs.value()) {
                REQUIRE(std::isfinite(*lhs.value()));
                REQUIRE(*lhs.value() >= 0.0);
                REQUIRE(*lhs.value() <= 100.0);
            }
        }
    }
}
