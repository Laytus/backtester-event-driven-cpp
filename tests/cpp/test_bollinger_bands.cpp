
#include "backtester/indicators/bollinger_bands.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using backtester::indicators::BollingerBands;
using backtester::indicators::BollingerBandsValue;

void require_unavailable(const BollingerBands& bands) {
    REQUIRE_FALSE(bands.value().has_value());
}

void require_bands(
    const BollingerBands& bands,
    double expected_middle,
    double expected_upper,
    double expected_lower,
    double tolerance = 1e-12
) {
    const auto result = bands.value();
    REQUIRE(result.has_value());
    REQUIRE(std::isfinite(result->middle_band));
    REQUIRE(std::isfinite(result->upper_band));
    REQUIRE(std::isfinite(result->lower_band));
    REQUIRE(result->lower_band <= result->middle_band);
    REQUIRE(result->middle_band <= result->upper_band);

    REQUIRE(result->middle_band ==
            Catch::Approx(expected_middle).epsilon(tolerance));
    REQUIRE(result->upper_band ==
            Catch::Approx(expected_upper).epsilon(tolerance));
    REQUIRE(result->lower_band ==
            Catch::Approx(expected_lower).epsilon(tolerance));
}

void require_same_value(
    const BollingerBandsValue& actual,
    const BollingerBandsValue& expected
) {
    REQUIRE(actual.middle_band == expected.middle_band);
    REQUIRE(actual.upper_band == expected.upper_band);
    REQUIRE(actual.lower_band == expected.lower_band);
}

// Independent two-pass reference calculation for moderate-magnitude inputs.
// long double is deliberately separate from the statistics module under test.
BollingerBandsValue reference_bands(
    const std::deque<double>& window,
    double multiplier
) {
    long double sum = 0.0L;
    for (const double x : window) {
        sum += static_cast<long double>(x);
    }

    const long double n = static_cast<long double>(window.size());
    const long double mean = sum / n;

    long double squared_deviations = 0.0L;
    for (const double x : window) {
        const long double deviation =
            static_cast<long double>(x) - mean;
        squared_deviations += deviation * deviation;
    }

    const long double deviation = std::sqrt(squared_deviations / n);
    const long double radius =
        static_cast<long double>(multiplier) * deviation;

    return {
        static_cast<double>(mean),
        static_cast<double>(mean + radius),
        static_cast<double>(mean - radius)
    };
}

}  // namespace

TEST_CASE("Bollinger Bands exposes default and custom configuration") {
    const BollingerBands defaults;
    REQUIRE(defaults.period() == 20);
    REQUIRE(defaults.multiplier() == 2.0);
    require_unavailable(defaults);

    const BollingerBands custom{7, 2.5};
    REQUIRE(custom.period() == 7);
    REQUIRE(custom.multiplier() == 2.5);
    require_unavailable(custom);
}

TEST_CASE("Bollinger Bands rejects zero period") {
    REQUIRE_THROWS_AS(BollingerBands(0), std::invalid_argument);
    REQUIRE_THROWS_AS(BollingerBands(0, 2.0), std::invalid_argument);
}

TEST_CASE("Bollinger Bands rejects nonpositive and nonfinite multipliers") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    for (const double invalid :
         {0.0, -0.0, -1.0, nan, infinity, -infinity}) {
        REQUIRE_THROWS_AS(
            BollingerBands(3, invalid),
            std::invalid_argument
        );
    }
}

TEST_CASE("Bollinger Bands default period first becomes ready at observation twenty") {
    BollingerBands bands;

    for (int i = 1; i < 20; ++i) {
        bands.update(static_cast<double>(i));
        require_unavailable(bands);
    }

    bands.update(20.0);
    const double sigma = std::sqrt(33.25);

    require_bands(
        bands,
        10.5,
        10.5 + 2.0 * sigma,
        10.5 - 2.0 * sigma
    );

    bands.update(21.0);
    require_bands(
        bands,
        11.5,
        11.5 + 2.0 * sigma,
        11.5 - 2.0 * sigma
    );
}

TEST_CASE("Bollinger Bands matches the approved five-price rolling oracle") {
    BollingerBands bands{3, 2.0};

    bands.update(100.0);
    require_unavailable(bands);

    bands.update(102.0);
    require_unavailable(bands);

    bands.update(104.0);
    const double sigma1 = std::sqrt(8.0 / 3.0);
    require_bands(
        bands,
        102.0,
        102.0 + 2.0 * sigma1,
        102.0 - 2.0 * sigma1
    );

    bands.update(103.0);
    const double sigma2 = std::sqrt(2.0 / 3.0);
    require_bands(
        bands,
        103.0,
        103.0 + 2.0 * sigma2,
        103.0 - 2.0 * sigma2
    );

    bands.update(107.0);
    const double middle = 314.0 / 3.0;
    const double sigma3 = std::sqrt(26.0) / 3.0;
    require_bands(
        bands,
        middle,
        middle + 2.0 * sigma3,
        middle - 2.0 * sigma3
    );
}

TEST_CASE("Bollinger Bands uses population rather than sample standard deviation") {
    BollingerBands bands{2, 2.0};
    bands.update(1.0);
    bands.update(3.0);

    // Mean=2, population sigma=1 (sample sigma would be sqrt(2)).
    require_bands(bands, 2.0, 4.0, 0.0);
}

TEST_CASE("Bollinger Bands multiplier changes the width but not the center") {
    BollingerBands narrow{3, 0.5};
    BollingerBands wide{3, 3.0};

    for (const double price : {7.0, 10.0, 13.0, 9.0}) {
        narrow.update(price);
        wide.update(price);
    }

    const auto a = narrow.value();
    const auto b = wide.value();

    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    REQUIRE(a->middle_band == b->middle_band);
    REQUIRE(
        (b->upper_band - b->lower_band) ==
        Catch::Approx(6.0 * (a->upper_band - a->lower_band))
    );
}

TEST_CASE("Bollinger Bands period one makes all bands equal to the current price") {
    BollingerBands bands{1, 5.0};

    for (const double price :
         {100.0, -30.0, 0.0, 1e150, -1e150}) {
        bands.update(price);
        require_bands(bands, price, price, price);
    }
}

TEST_CASE("Bollinger Bands constant windows have zero width") {
    BollingerBands bands{4};

    for (int i = 0; i < 12; ++i) {
        bands.update(-7.25);

        if (i < 3) {
            require_unavailable(bands);
        } else {
            require_bands(bands, -7.25, -7.25, -7.25);
        }
    }
}

TEST_CASE("Bollinger Bands handles all-zero observations") {
    BollingerBands bands{3};

    for (int i = 0; i < 8; ++i) {
        bands.update(0.0);
    }

    require_bands(bands, 0.0, 0.0, 0.0);
}

TEST_CASE("Bollinger Bands accepts negative and mixed-sign prices") {
    BollingerBands bands{3, 1.0};

    bands.update(-4.0);
    bands.update(0.0);
    bands.update(4.0);

    const double sigma = std::sqrt(32.0 / 3.0);
    require_bands(bands, 0.0, sigma, -sigma);

    bands.update(-2.0);  // Window is [0, 4, -2].

    const double shifted_sigma = std::sqrt(56.0 / 9.0);

    require_bands(
        bands,
        2.0 / 3.0,
        2.0 / 3.0 + shifted_sigma,
        2.0 / 3.0 - shifted_sigma
    );
}

TEST_CASE("Bollinger Bands forgets expired highs and lows") {
    BollingerBands bands{3, 1.0};

    for (const double price : {0.0, 0.0, 30.0}) {
        bands.update(price);
    }

    require_bands(
        bands,
        10.0,
        10.0 + std::sqrt(200.0),
        10.0 - std::sqrt(200.0)
    );

    bands.update(0.0);  // [0, 30, 0], same moments.

    require_bands(
        bands,
        10.0,
        10.0 + std::sqrt(200.0),
        10.0 - std::sqrt(200.0)
    );

    bands.update(0.0);  // [30, 0, 0], same moments.

    require_bands(
        bands,
        10.0,
        10.0 + std::sqrt(200.0),
        10.0 - std::sqrt(200.0)
    );

    bands.update(0.0);  // [0, 0, 0], expired high.
    require_bands(bands, 0.0, 0.0, 0.0);
}

TEST_CASE("Bollinger Bands value and configuration queries never modify state") {
    BollingerBands bands{2, 1.5};

    require_unavailable(bands);
    require_unavailable(bands);

    REQUIRE(bands.period() == 2);
    REQUIRE(bands.multiplier() == 1.5);

    bands.update(3.0);
    require_unavailable(bands);
    require_unavailable(bands);

    bands.update(5.0);
    const auto first = bands.value();
    REQUIRE(first.has_value());

    for (int i = 0; i < 10; ++i) {
        const auto again = bands.value();
        REQUIRE(again.has_value());
        require_same_value(*again, *first);
    }

    bands.update(7.0);
    require_bands(bands, 6.0, 7.5, 4.5);
}

TEST_CASE("Bollinger Bands rejects nonfinite observations before its first price") {
    BollingerBands bands{2};

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    for (const double invalid : {nan, infinity, -infinity}) {
        REQUIRE_THROWS_AS(
            bands.update(invalid),
            std::invalid_argument
        );
        require_unavailable(bands);
    }

    bands.update(3.0);
    require_unavailable(bands);

    bands.update(3.0);
    require_bands(bands, 3.0, 3.0, 3.0);
}

TEST_CASE("Bollinger Bands rejects invalid observations without advancing warmup") {
    BollingerBands bands{3};
    const double nan = std::numeric_limits<double>::quiet_NaN();

    bands.update(2.0);
    REQUIRE_THROWS_AS(bands.update(nan), std::invalid_argument);

    bands.update(4.0);
    require_unavailable(bands);

    bands.update(6.0);
    const double sigma = std::sqrt(8.0 / 3.0);

    require_bands(
        bands,
        4.0,
        4.0 + 2.0 * sigma,
        4.0 - 2.0 * sigma
    );
}

TEST_CASE("Bollinger Bands invalid input just before readiness does not change the seed") {
    BollingerBands bands{3, 1.0};

    bands.update(1.0);
    bands.update(3.0);

    REQUIRE_THROWS_AS(
        bands.update(std::numeric_limits<double>::infinity()),
        std::invalid_argument
    );

    require_unavailable(bands);

    bands.update(5.0);
    const double sigma = std::sqrt(8.0 / 3.0);

    require_bands(
        bands,
        3.0,
        3.0 + sigma,
        3.0 - sigma
    );
}

TEST_CASE("Bollinger Bands rejects nonfinite observations without changing a ready result") {
    BollingerBands bands{2, 2.0};

    bands.update(10.0);
    bands.update(14.0);

    const auto original = bands.value();
    REQUIRE(original.has_value());

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    for (const double invalid : {nan, infinity, -infinity}) {
        REQUIRE_THROWS_AS(
            bands.update(invalid),
            std::invalid_argument
        );

        const auto actual = bands.value();
        REQUIRE(actual.has_value());
        require_same_value(*actual, *original);
    }

    bands.update(18.0);  // Correct next window: [14, 18].
    require_bands(bands, 16.0, 20.0, 12.0);
}

TEST_CASE("Bollinger Bands handles constant maximum-magnitude finite prices") {
    const double huge = std::numeric_limits<double>::max();

    BollingerBands positive{3, 3.0};
    BollingerBands negative{3, 3.0};

    for (int i = 0; i < 4; ++i) {
        positive.update(huge);
        negative.update(-huge);
    }

    require_bands(positive, huge, huge, huge);
    require_bands(negative, -huge, -huge, -huge);
}

TEST_CASE("Bollinger Bands handles large opposing values without squared overflow") {
    const double huge = 1e308;

    BollingerBands bands{2, 1.0};
    bands.update(-huge);
    bands.update(huge);

    require_bands(bands, 0.0, huge, -huge);
}

TEST_CASE("Bollinger Bands retains narrow dispersion around a large baseline") {
    const double baseline = 1e16;

    BollingerBands bands{2, 2.0};
    bands.update(baseline);
    bands.update(baseline + 2.0);

    const auto result = bands.value();

    REQUIRE(result.has_value());
    REQUIRE(result->middle_band == Catch::Approx(baseline));
    REQUIRE(result->upper_band - result->lower_band == 4.0);
}

TEST_CASE("Bollinger Bands failed first ready calculation does not commit warmup") {
    const double huge = 1e308;

    BollingerBands bands{2, 4.0};
    bands.update(huge);
    require_unavailable(bands);

    REQUIRE_THROWS_AS(
        bands.update(-huge),
        std::overflow_error
    );

    require_unavailable(bands);

    bands.update(huge);  // Window must be [huge, huge].
    require_bands(bands, huge, huge, huge);
}

TEST_CASE("Bollinger Bands failed rolling overflow preserves its complete state") {
    BollingerBands bands{2, 4.0};

    bands.update(1.0);
    bands.update(1.0);

    const auto before = bands.value();
    REQUIRE(before.has_value());

    REQUIRE_THROWS_AS(
        bands.update(1e308),
        std::overflow_error
    );

    const auto after = bands.value();
    REQUIRE(after.has_value());
    require_same_value(*before, *after);

    bands.update(3.0);  // Previous window must still be [1, 1].
    require_bands(bands, 2.0, 6.0, -2.0);
}

TEST_CASE("Bollinger Bands agrees with independent rolling reference across periods") {
    std::mt19937 generator(20261024);
    std::uniform_real_distribution<double> distribution(-1000.0, 1000.0);

    std::vector<double> observations(250);

    for (double& x : observations) {
        x = distribution(generator);
    }

    for (const std::size_t period :
         {1U, 2U, 3U, 5U, 10U, 20U, 50U}) {
        BollingerBands bands{period, 1.75};
        std::deque<double> window;

        for (const double x : observations) {
            bands.update(x);
            window.push_back(x);

            if (window.size() > period) {
                window.pop_front();
            }

            if (window.size() < period) {
                require_unavailable(bands);
            } else {
                const auto expected = reference_bands(window, 1.75);

                require_bands(
                    bands,
                    expected.middle_band,
                    expected.upper_band,
                    expected.lower_band,
                    1e-11
                );
            }
        }
    }
}

TEST_CASE("Bollinger Bands remains correct over a long increasing sequence") {
    constexpr std::size_t period = 5;

    BollingerBands bands{period, 2.0};

    const double sigma = std::sqrt(2.0);

    for (int i = 0; i < 5000; ++i) {
        bands.update(static_cast<double>(i));

        if (i < static_cast<int>(period - 1)) {
            require_unavailable(bands);
        } else {
            const double middle = static_cast<double>(i - 2);

            require_bands(
                bands,
                middle,
                middle + 2.0 * sigma,
                middle - 2.0 * sigma
            );
        }
    }
}

TEST_CASE("Bollinger Bands is deterministic for identical observations and configurations") {
    BollingerBands first{7, 1.25};
    BollingerBands second{7, 1.25};

    const std::vector<double> observations{
        102.0, 101.0, 104.0, 99.0, 100.0,
        97.0, 103.0, 110.0, 105.0, 108.0,
        107.0, 103.0, 99.0, 101.0, 102.0
    };

    for (const double x : observations) {
        first.update(x);
        second.update(x);

        const auto a = first.value();
        const auto b = second.value();

        REQUIRE(a.has_value() == b.has_value());

        if (a) {
            require_same_value(*a, *b);
        }
    }
}
