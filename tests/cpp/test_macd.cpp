#include "backtester/indicators/macd.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

using backtester::indicators::MACD;
using backtester::indicators::MACDValue;

void require_unavailable(const MACDValue& result) {
    REQUIRE_FALSE(result.macd_line.has_value());
    REQUIRE_FALSE(result.signal_line.has_value());
    REQUIRE_FALSE(result.histogram.has_value());
}

void require_macd_only(const MACDValue& result, double expected) {
    REQUIRE(result.macd_line.has_value());
    REQUIRE(*result.macd_line == Catch::Approx(expected));
    REQUIRE_FALSE(result.signal_line.has_value());
    REQUIRE_FALSE(result.histogram.has_value());
}

void require_complete(
    const MACDValue& result,
    double expected_macd,
    double expected_signal,
    double expected_histogram
) {
    REQUIRE(result.macd_line.has_value());
    REQUIRE(result.signal_line.has_value());
    REQUIRE(result.histogram.has_value());

    REQUIRE(*result.macd_line == Catch::Approx(expected_macd).margin(1e-12));
    REQUIRE(*result.signal_line == Catch::Approx(expected_signal).margin(1e-12));
    REQUIRE(*result.histogram == Catch::Approx(expected_histogram).margin(1e-12));
}

void require_same_values(const MACDValue& lhs, const MACDValue& rhs) {
    REQUIRE(lhs.macd_line == rhs.macd_line);
    REQUIRE(lhs.signal_line == rhs.signal_line);
    REQUIRE(lhs.histogram == rhs.histogram);
}

}  // namespace

TEST_CASE("MACD exposes default periods and effective smoothing factors") {
    const MACD macd;

    REQUIRE(macd.fast_period() == 12);
    REQUIRE(macd.slow_period() == 26);
    REQUIRE(macd.signal_period() == 9);
    REQUIRE(macd.fast_alpha() == Catch::Approx(2.0 / 13.0));
    REQUIRE(macd.slow_alpha() == Catch::Approx(2.0 / 27.0));
    REQUIRE(macd.signal_alpha() == Catch::Approx(2.0 / 10.0));
    require_unavailable(macd.value());

    const MACD custom{3, 8, 4};
    REQUIRE(custom.fast_period() == 3);
    REQUIRE(custom.slow_period() == 8);
    REQUIRE(custom.signal_period() == 4);
    REQUIRE(custom.fast_alpha() == Catch::Approx(0.5));
    REQUIRE(custom.slow_alpha() == Catch::Approx(2.0 / 9.0));
    REQUIRE(custom.signal_alpha() == Catch::Approx(0.4));
    require_unavailable(custom.value());
}

TEST_CASE("MACD rejects zero and unordered periods") {
    REQUIRE_THROWS_AS(MACD(0, 3, 2), std::invalid_argument);
    REQUIRE_THROWS_AS(MACD(1, 0, 2), std::invalid_argument);
    REQUIRE_THROWS_AS(MACD(1, 2, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(MACD(3, 3, 2), std::invalid_argument);
    REQUIRE_THROWS_AS(MACD(4, 3, 2), std::invalid_argument);
}

TEST_CASE("MACD rejects invalid alpha overrides for each constituent EMA") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    REQUIRE_THROWS_AS(MACD(2, 3, 2, 0.0), std::invalid_argument);
    REQUIRE_THROWS_AS(MACD(2, 3, 2, -0.1), std::invalid_argument);
    REQUIRE_THROWS_AS(MACD(2, 3, 2, 1.01), std::invalid_argument);
    REQUIRE_THROWS_AS(MACD(2, 3, 2, nan), std::invalid_argument);
    REQUIRE_THROWS_AS(MACD(2, 3, 2, inf), std::invalid_argument);
    REQUIRE_THROWS_AS(MACD(2, 3, 2, -inf), std::invalid_argument);

    REQUIRE_THROWS_AS(MACD(2, 3, 2, std::nullopt, 0.0), std::invalid_argument);
    REQUIRE_THROWS_AS(MACD(2, 3, 2, std::nullopt, 1.01), std::invalid_argument);
    REQUIRE_THROWS_AS(MACD(2, 3, 2, std::nullopt, nan), std::invalid_argument);
    REQUIRE_THROWS_AS(MACD(2, 3, 2, std::nullopt, -inf), std::invalid_argument);

    REQUIRE_THROWS_AS(
        MACD(2, 3, 2, std::nullopt, std::nullopt, 0.0),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        MACD(2, 3, 2, std::nullopt, std::nullopt, 1.01),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        MACD(2, 3, 2, std::nullopt, std::nullopt, nan),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        MACD(2, 3, 2, std::nullopt, std::nullopt, inf),
        std::invalid_argument
    );
}

TEST_CASE("MACD permits independently configured alphas without ordering them") {
    const MACD macd{2, 3, 2, 0.1, 0.9, 1.0};

    REQUIRE(macd.fast_alpha() == Catch::Approx(0.1));
    REQUIRE(macd.slow_alpha() == Catch::Approx(0.9));
    REQUIRE(macd.signal_alpha() == Catch::Approx(1.0));
    REQUIRE(macd.fast_alpha() < macd.slow_alpha());
}

TEST_CASE("MACD default first becomes available at 26 and complete at 34") {
    MACD macd;

    for (int observation = 1; observation <= 25; ++observation) {
        macd.update(100.0);
        require_unavailable(macd.value());
    }

    macd.update(100.0);  // Observation 26: first MACD line.
    require_macd_only(macd.value(), 0.0);

    for (int observation = 27; observation <= 33; ++observation) {
        macd.update(100.0);
        require_macd_only(macd.value(), 0.0);
    }

    macd.update(100.0);  // Observation 34: first signal and histogram.
    require_complete(macd.value(), 0.0, 0.0, 0.0);
}

TEST_CASE("MACD matches the approved SMA-seeded mathematical oracle") {
    MACD macd{2, 3, 2};

    macd.update(10.0);
    require_unavailable(macd.value());

    macd.update(12.0);
    require_unavailable(macd.value());

    macd.update(11.0);
    require_macd_only(macd.value(), 0.0);

    macd.update(15.0);
    require_complete(macd.value(), 2.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0);

    macd.update(13.0);
    require_complete(macd.value(), 2.0 / 9.0, 7.0 / 27.0, -1.0 / 27.0);
}

TEST_CASE("MACD custom alphas modify recurrences but not SMA seeds") {
    MACD macd{2, 3, 2, 0.5, 0.25, 0.5};

    macd.update(10.0);
    macd.update(12.0);
    require_unavailable(macd.value());

    macd.update(11.0);  // Fast seed 11; slow seed 11.
    require_macd_only(macd.value(), 0.0);

    macd.update(15.0);  // Fast 13; slow 12; signal seed (0+1)/2.
    require_complete(macd.value(), 1.0, 0.5, 0.5);

    macd.update(13.0);  // Fast 13; slow 12.25; signal 0.625.
    require_complete(macd.value(), 0.75, 0.625, 0.125);
}

TEST_CASE("MACD alpha one still uses SMA seeds before direct tracking") {
    MACD macd{2, 3, 2, 1.0, 1.0, 1.0};

    macd.update(10.0);
    macd.update(12.0);
    require_unavailable(macd.value());

    macd.update(11.0);
    require_macd_only(macd.value(), 0.0);

    macd.update(15.0);
    require_complete(macd.value(), 0.0, 0.0, 0.0);

    macd.update(13.0);
    require_complete(macd.value(), 0.0, 0.0, 0.0);
}

TEST_CASE("MACD signal alpha one follows the latest MACD after its seed") {
    MACD macd{2, 3, 2, std::nullopt, std::nullopt, 1.0};

    for (const double observation : {10.0, 12.0, 11.0}) {
        macd.update(observation);
    }
    require_macd_only(macd.value(), 0.0);

    macd.update(15.0);
    require_complete(macd.value(), 2.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0);

    macd.update(13.0);
    require_complete(macd.value(), 2.0 / 9.0, 2.0 / 9.0, 0.0);
}

TEST_CASE("MACD signal period one is complete with its first MACD line") {
    MACD macd{2, 3, 1};

    macd.update(10.0);
    require_unavailable(macd.value());
    macd.update(12.0);
    require_unavailable(macd.value());

    macd.update(11.0);
    require_complete(macd.value(), 0.0, 0.0, 0.0);

    macd.update(15.0);
    require_complete(macd.value(), 2.0 / 3.0, 2.0 / 3.0, 0.0);
}

TEST_CASE("MACD fast period one is ready according to slow and signal periods") {
    MACD macd{1, 2, 3};

    macd.update(100.0);
    require_unavailable(macd.value());

    macd.update(104.0);  // Fast 104; slow seed 102; MACD 2.
    require_macd_only(macd.value(), 2.0);

    macd.update(102.0);  // Fast 102; slow 102; MACD 0.
    require_macd_only(macd.value(), 0.0);

    macd.update(108.0);  // Fast 108; slow 106; MACD 2; signal seed 4/3.
    require_complete(macd.value(), 2.0, 4.0 / 3.0, 2.0 / 3.0);
}

TEST_CASE("MACD accepts finite zero and negative prices") {
    MACD negative{2, 3, 2};

    for (const double observation : {-10.0, -12.0, -11.0}) {
        negative.update(observation);
    }
    require_macd_only(negative.value(), 0.0);

    negative.update(-15.0);
    require_complete(negative.value(), -2.0 / 3.0, -1.0 / 3.0, -1.0 / 3.0);

    MACD zeros{2, 3, 2};
    for (int i = 0; i < 4; ++i) {
        zeros.update(0.0);
    }
    require_complete(zeros.value(), 0.0, 0.0, 0.0);
}

TEST_CASE("MACD value queries and parameter accessors do not change state") {
    MACD macd{2, 3, 2};
    for (const double observation : {10.0, 12.0, 11.0, 15.0}) {
        macd.update(observation);
    }

    const MACDValue first = macd.value();
    const MACDValue second = macd.value();
    require_same_values(first, second);
    require_complete(first, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0);

    REQUIRE(macd.fast_period() == 2);
    REQUIRE(macd.slow_period() == 3);
    REQUIRE(macd.signal_period() == 2);
    REQUIRE(macd.fast_alpha() == Catch::Approx(2.0 / 3.0));
    REQUIRE(macd.slow_alpha() == Catch::Approx(0.5));
    REQUIRE(macd.signal_alpha() == Catch::Approx(2.0 / 3.0));

    macd.update(13.0);
    require_complete(macd.value(), 2.0 / 9.0, 7.0 / 27.0, -1.0 / 27.0);
}

TEST_CASE("MACD rejects non-finite observations without advancing initial warm-up") {
    MACD macd{2, 3, 2};
    macd.update(10.0);

    for (const double bad : {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    }) {
        REQUIRE_THROWS_AS(macd.update(bad), std::invalid_argument);
        require_unavailable(macd.value());
    }

    macd.update(12.0);
    require_unavailable(macd.value());
    macd.update(11.0);
    require_macd_only(macd.value(), 0.0);
}

TEST_CASE("MACD invalid observations do not advance a partially ready signal") {
    MACD macd{2, 3, 3};
    for (const double price : {10.0, 12.0, 11.0}) {
        macd.update(price);
    }
    require_macd_only(macd.value(), 0.0);

    REQUIRE_THROWS_AS(
        macd.update(std::numeric_limits<double>::infinity()),
        std::invalid_argument
    );
    require_macd_only(macd.value(), 0.0);

    macd.update(15.0);
    require_macd_only(macd.value(), 2.0 / 3.0);

    REQUIRE_THROWS_AS(
        macd.update(std::numeric_limits<double>::quiet_NaN()),
        std::invalid_argument
    );
    require_macd_only(macd.value(), 2.0 / 3.0);

    macd.update(13.0);  // Third valid MACD observation initializes signal.
    require_complete(macd.value(), 2.0 / 9.0, 8.0 / 27.0, -2.0 / 27.0);
}

TEST_CASE("MACD rejects non-finite observations without changing complete state") {
    MACD macd{2, 3, 2};
    for (const double price : {10.0, 12.0, 11.0, 15.0}) {
        macd.update(price);
    }
    const MACDValue before = macd.value();

    for (const double bad : {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    }) {
        REQUIRE_THROWS_AS(macd.update(bad), std::invalid_argument);
        require_same_values(macd.value(), before);
    }

    macd.update(13.0);
    require_complete(macd.value(), 2.0 / 9.0, 7.0 / 27.0, -1.0 / 27.0);
}

TEST_CASE("MACD line overflow preserves every EMA and the partial result") {
    MACD macd{1, 2, 2, std::nullopt, 0.01};
    constexpr double large = 1e308;

    macd.update(-large);
    macd.update(-large);
    require_macd_only(macd.value(), 0.0);

    REQUIRE_THROWS_AS(macd.update(large), std::overflow_error);
    require_macd_only(macd.value(), 0.0);

    // The rejected price must not enter any EMA or signal warm-up.
    macd.update(-large);
    require_complete(macd.value(), 0.0, 0.0, 0.0);
}

TEST_CASE("MACD histogram overflow rolls back a fully ready composite state") {
    // A long positive run creates a positive signal. Switching the fast EMA
    // to a negative price leaves a finite MACD line, but their difference
    // overflows when the histogram is calculated.
    constexpr double large = 9e307;
    MACD actual{1, 2, 1, std::nullopt, 0.02, 0.03};
    MACD control{1, 2, 1, std::nullopt, 0.02, 0.03};

    for (const double price : {-large, -large}) {
        actual.update(price);
        control.update(price);
    }
    for (int i = 0; i < 47; ++i) {
        actual.update(large);
        control.update(large);
    }

    require_same_values(actual.value(), control.value());
    REQUIRE(actual.value().histogram.has_value());
    REQUIRE(std::isfinite(*actual.value().histogram));

    REQUIRE_THROWS_AS(actual.update(-large), std::overflow_error);
    require_same_values(actual.value(), control.value());

    // Continued execution must match an untouched reference instance.
    actual.update(large);
    control.update(large);
    require_same_values(actual.value(), control.value());
}

TEST_CASE("MACD processes large finite constant prices without spurious overflow") {
    MACD macd;
    const double large = std::numeric_limits<double>::max();

    for (int i = 0; i < 34; ++i) {
        REQUIRE_NOTHROW(macd.update(large));
    }

    require_complete(macd.value(), 0.0, 0.0, 0.0);
}

TEST_CASE("MACD is deterministic for identical observations and configurations") {
    MACD first{2, 3, 2, 0.5, 0.25, 0.5};
    MACD second{2, 3, 2, 0.5, 0.25, 0.5};

    for (const double price : {10.0, 12.0, 11.0, 15.0, 13.0, -5.0, 7.0}) {
        first.update(price);
        second.update(price);
        require_same_values(first.value(), second.value());
        REQUIRE(first.fast_alpha() == second.fast_alpha());
        REQUIRE(first.slow_alpha() == second.slow_alpha());
        REQUIRE(first.signal_alpha() == second.signal_alpha());
    }
}
