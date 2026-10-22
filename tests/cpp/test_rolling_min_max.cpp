#include "backtester/indicators/rolling_min_max.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using backtester::indicators::RollingMinMax;

void require_unavailable(const RollingMinMax& indicator) {
    REQUIRE_FALSE(indicator.value().has_value());
}

void require_extrema(
    const RollingMinMax& indicator,
    double expected_minimum,
    double expected_maximum
) {
    const auto actual = indicator.value();
    REQUIRE(actual.has_value());
    REQUIRE(actual->minimum == expected_minimum);
    REQUIRE(actual->maximum == expected_maximum);
}

}  // namespace

TEST_CASE("Rolling Min/Max validates its period and exposes its configuration") {
    REQUIRE_THROWS_AS(RollingMinMax(0), std::invalid_argument);

    const RollingMinMax default_indicator;
    REQUIRE(default_indicator.period() == 20);
    require_unavailable(default_indicator);

    const RollingMinMax custom_indicator{5};
    REQUIRE(custom_indicator.period() == 5);
    require_unavailable(custom_indicator);
}

TEST_CASE("Rolling Min/Max default period first becomes ready at observation twenty") {
    RollingMinMax indicator;

    for (int price = 1; price < 20; ++price) {
        indicator.update(static_cast<double>(price));
        require_unavailable(indicator);
    }

    indicator.update(20.0);
    require_extrema(indicator, 1.0, 20.0);

    indicator.update(21.0);
    require_extrema(indicator, 2.0, 21.0);
}

TEST_CASE("Rolling Min/Max matches the approved six-observation oracle") {
    RollingMinMax indicator{3};
    const std::vector<double> prices{5.0, 3.0, 3.0, 7.0, 2.0, 6.0};
    const std::vector<double> minima{3.0, 3.0, 2.0, 2.0};
    const std::vector<double> maxima{5.0, 7.0, 7.0, 7.0};

    for (std::size_t i = 0; i < prices.size(); ++i) {
        indicator.update(prices[i]);

        if (i < 2) {
            require_unavailable(indicator);
        } else {
            require_extrema(indicator, minima[i - 2], maxima[i - 2]);
        }
    }
}

TEST_CASE("Rolling Min/Max period one tracks the current observation") {
    RollingMinMax indicator{1};
    require_unavailable(indicator);

    for (const double price : {7.0, -3.0, 0.0, 7.0, -8.0}) {
        indicator.update(price);
        require_extrema(indicator, price, price);
    }
}

TEST_CASE("Rolling Min/Max handles a constant series with duplicate candidates") {
    RollingMinMax indicator{4};

    for (int i = 0; i < 30; ++i) {
        indicator.update(5.0);

        if (i < 3) {
            require_unavailable(indicator);
        } else {
            require_extrema(indicator, 5.0, 5.0);
        }
    }
}

TEST_CASE("Rolling Min/Max keeps the newest equal minimum until it expires") {
    RollingMinMax indicator{3};

    indicator.update(4.0);
    indicator.update(1.0);
    indicator.update(1.0);
    require_extrema(indicator, 1.0, 4.0);

    indicator.update(3.0);
    require_extrema(indicator, 1.0, 3.0);

    indicator.update(5.0);
    require_extrema(indicator, 1.0, 5.0);

    indicator.update(2.0);
    require_extrema(indicator, 2.0, 5.0);
}

TEST_CASE("Rolling Min/Max expires an old minimum in an increasing series") {
    RollingMinMax indicator{3};

    for (int i = 1; i <= 8; ++i) {
        indicator.update(static_cast<double>(i));

        if (i < 3) {
            require_unavailable(indicator);
        } else {
            require_extrema(
                indicator,
                static_cast<double>(i - 2),
                static_cast<double>(i)
            );
        }
    }
}

TEST_CASE("Rolling Min/Max expires an old maximum in a decreasing series") {
    RollingMinMax indicator{3};

    for (int i = 8; i >= 1; --i) {
        indicator.update(static_cast<double>(i));

        if (i <= 6) {
            require_extrema(
                indicator,
                static_cast<double>(i),
                static_cast<double>(i + 2)
            );
        } else {
            require_unavailable(indicator);
        }
    }
}

TEST_CASE("Rolling Min/Max expires extrema correctly in an oscillating series") {
    RollingMinMax indicator{3};
    const std::vector<double> prices{4, 9, 2, 8, 1, 7, 3, 6};
    const std::vector<double> minima{2, 2, 1, 1, 1, 3};
    const std::vector<double> maxima{9, 9, 8, 8, 7, 7};

    for (std::size_t i = 0; i < prices.size(); ++i) {
        indicator.update(prices[i]);
        if (i < 2) {
            require_unavailable(indicator);
        } else {
            require_extrema(indicator, minima[i - 2], maxima[i - 2]);
        }
    }
}

TEST_CASE("Rolling Min/Max removes multiple dominated minimum candidates") {
    RollingMinMax indicator{5};

    for (const double price : {2.0, 4.0, 6.0, 8.0, 10.0}) {
        indicator.update(price);
    }
    require_extrema(indicator, 2.0, 10.0);

    indicator.update(1.0);  // Dominates every candidate for the minimum.
    require_extrema(indicator, 1.0, 10.0);

    indicator.update(11.0);
    require_extrema(indicator, 1.0, 11.0);
}

TEST_CASE("Rolling Min/Max removes multiple dominated maximum candidates") {
    RollingMinMax indicator{5};

    for (const double price : {10.0, 8.0, 6.0, 4.0, 2.0}) {
        indicator.update(price);
    }
    require_extrema(indicator, 2.0, 10.0);

    indicator.update(11.0);  // Dominates every candidate for the maximum.
    require_extrema(indicator, 2.0, 11.0);

    indicator.update(-1.0);
    require_extrema(indicator, -1.0, 11.0);
}

TEST_CASE("Rolling Min/Max accepts negative values and zero") {
    RollingMinMax indicator{2};

    indicator.update(-5.0);
    require_unavailable(indicator);

    indicator.update(0.0);
    require_extrema(indicator, -5.0, 0.0);

    indicator.update(-2.0);
    require_extrema(indicator, -2.0, 0.0);

    indicator.update(3.0);
    require_extrema(indicator, -2.0, 3.0);

    indicator.update(-1.0);
    require_extrema(indicator, -1.0, 3.0);
}

TEST_CASE("Rolling Min/Max handles extreme finite values without arithmetic overflow") {
    RollingMinMax indicator{2};
    const double largest = std::numeric_limits<double>::max();

    indicator.update(-largest);
    indicator.update(largest);
    require_extrema(indicator, -largest, largest);

    indicator.update(0.0);
    require_extrema(indicator, 0.0, largest);

    indicator.update(-largest);
    require_extrema(indicator, -largest, 0.0);

    indicator.update(largest);
    require_extrema(indicator, -largest, largest);
}

TEST_CASE("Rolling Min/Max rejects non-finite inputs without advancing warm-up") {
    RollingMinMax indicator{3};
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    indicator.update(4.0);
    REQUIRE_THROWS_AS(indicator.update(nan), std::invalid_argument);
    REQUIRE_THROWS_AS(indicator.update(inf), std::invalid_argument);
    REQUIRE_THROWS_AS(indicator.update(-inf), std::invalid_argument);
    require_unavailable(indicator);

    indicator.update(2.0);
    require_unavailable(indicator);

    indicator.update(6.0);
    require_extrema(indicator, 2.0, 6.0);
}

TEST_CASE("Rolling Min/Max rejects non-finite inputs without changing ready extrema") {
    RollingMinMax indicator{3};
    indicator.update(8.0);
    indicator.update(4.0);
    indicator.update(7.0);
    require_extrema(indicator, 4.0, 8.0);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    for (const double bad : {nan, inf, -inf}) {
        REQUIRE_THROWS_AS(indicator.update(bad), std::invalid_argument);
        require_extrema(indicator, 4.0, 8.0);
    }

    indicator.update(5.0);
    require_extrema(indicator, 4.0, 7.0);
}

TEST_CASE("Rolling Min/Max invalid inputs do not prematurely expire old candidates") {
    RollingMinMax indicator{2};
    indicator.update(0.0);
    indicator.update(9.0);
    require_extrema(indicator, 0.0, 9.0);

    for (int i = 0; i < 5; ++i) {
        REQUIRE_THROWS_AS(
            indicator.update(std::numeric_limits<double>::quiet_NaN()),
            std::invalid_argument
        );
        require_extrema(indicator, 0.0, 9.0);
    }

    indicator.update(7.0);  // Only the successful updates count.
    require_extrema(indicator, 7.0, 9.0);
}

TEST_CASE("Rolling Min/Max rejects invalid data just before reaching readiness") {
    RollingMinMax indicator{3};
    indicator.update(4.0);
    indicator.update(8.0);
    require_unavailable(indicator);

    REQUIRE_THROWS_AS(
        indicator.update(std::numeric_limits<double>::infinity()),
        std::invalid_argument
    );
    require_unavailable(indicator);

    indicator.update(2.0);
    require_extrema(indicator, 2.0, 8.0);
}

TEST_CASE("Rolling Min/Max value and period queries never change state") {
    RollingMinMax indicator{3};
    const RollingMinMax& observation = indicator;

    for (int i = 0; i < 5; ++i) {
        REQUIRE(observation.period() == 3);
        require_unavailable(observation);
    }

    indicator.update(4.0);
    indicator.update(1.0);
    indicator.update(5.0);

    for (int i = 0; i < 5; ++i) {
        REQUIRE(observation.period() == 3);
        require_extrema(observation, 1.0, 5.0);
    }

    indicator.update(3.0);
    require_extrema(observation, 1.0, 5.0);
}

TEST_CASE("Rolling Min/Max independent high and low streams support prior-bar channels") {
    RollingMinMax highs{3};
    RollingMinMax lows{3};

    for (const double high : {10.0, 12.0, 11.0}) {
        highs.update(high);
    }
    for (const double low : {6.0, 7.0, 5.0}) {
        lows.update(low);
    }

    // Read the historical channel BEFORE incorporating the current bar.
    const auto previous_high_channel = highs.value();
    const auto previous_low_channel = lows.value();
    REQUIRE(previous_high_channel.has_value());
    REQUIRE(previous_low_channel.has_value());
    REQUIRE(13.0 > previous_high_channel->maximum);
    REQUIRE(previous_high_channel->maximum == 12.0);
    REQUIRE(previous_low_channel->minimum == 5.0);

    highs.update(14.0);
    lows.update(9.0);
    require_extrema(highs, 11.0, 14.0);
    require_extrema(lows, 5.0, 9.0);
}

TEST_CASE("Rolling Min/Max is deterministic for identical observation sequences") {
    RollingMinMax first{7};
    RollingMinMax second{7};

    for (const double price : {5.0, 2.0, 2.0, -5.0, 8.0, 3.0, 10.0,
                               -1.0, -1.0, 9.0, 0.0, 4.0, -7.0, 6.0}) {
        first.update(price);
        second.update(price);

        const auto left = first.value();
        const auto right = second.value();
        REQUIRE(left.has_value() == right.has_value());
        if (left) {
            REQUIRE(left->minimum == right->minimum);
            REQUIRE(left->maximum == right->maximum);
        }
    }
}

TEST_CASE("Rolling Min/Max matches an independent brute-force oracle across periods") {
    // Compare the O(1)-amortized implementation with a naive O(N) window scan.
    for (const std::size_t period : {1U, 2U, 3U, 7U, 20U, 32U, 64U}) {
        RollingMinMax indicator{period};
        std::mt19937 engine{20261022U + static_cast<unsigned int>(period)};
        std::uniform_int_distribution<int> distribution{-50, 50};
        std::vector<double> history;

        for (int i = 0; i < 500; ++i) {
            const double price = static_cast<double>(distribution(engine));
            history.push_back(price);
            indicator.update(price);

            if (history.size() < period) {
                require_unavailable(indicator);
                continue;
            }

            const auto begin = history.end() - static_cast<std::ptrdiff_t>(period);
            const auto [minimum, maximum] = std::minmax_element(begin, history.end());
            require_extrema(indicator, *minimum, *maximum);
        }
    }
}

TEST_CASE("Rolling Min/Max remains correct over long monotonic streams") {
    constexpr int period = 32;
    RollingMinMax increasing{period};
    RollingMinMax decreasing{period};

    for (int i = 0; i < 5000; ++i) {
        increasing.update(static_cast<double>(i));
        decreasing.update(-static_cast<double>(i));

        if (i < period - 1) {
            require_unavailable(increasing);
            require_unavailable(decreasing);
        } else if (i % 257 == 0 || i == 4999) {
            require_extrema(
                increasing,
                static_cast<double>(i - period + 1),
                static_cast<double>(i)
            );
            require_extrema(
                decreasing,
                -static_cast<double>(i),
                -static_cast<double>(i - period + 1)
            );
        }
    }
}
