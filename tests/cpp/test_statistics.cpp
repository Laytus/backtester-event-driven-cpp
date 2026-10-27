#include "backtester/statistics/statistics.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

using backtester::statistics::population_moments;
using backtester::statistics::sample_moments;

template <typename Moments>
void require_moments(
    const Moments& actual,
    double expected_mean,
    double expected_standard_deviation,
    double relative_tolerance = 1e-12
) {
    REQUIRE(std::isfinite(actual.mean));
    REQUIRE(std::isfinite(actual.standard_deviation));
    REQUIRE(actual.standard_deviation >= 0.0);

    if (expected_mean == 0.0) {
        REQUIRE(actual.mean == 0.0);
    } else {
        REQUIRE(actual.mean == Catch::Approx(expected_mean).epsilon(relative_tolerance));
    }

    if (expected_standard_deviation == 0.0) {
        REQUIRE(actual.standard_deviation == 0.0);
    } else {
        REQUIRE(
            actual.standard_deviation ==
            Catch::Approx(expected_standard_deviation).epsilon(relative_tolerance)
        );
    }
}

void require_matching_containers(const std::vector<double>& observations) {
    const std::deque<double> deque(observations.begin(), observations.end());

    const auto contiguous = population_moments(std::span<const double>(observations));
    const auto noncontiguous = population_moments(deque);

    REQUIRE(contiguous.mean == noncontiguous.mean);
    REQUIRE(contiguous.standard_deviation == noncontiguous.standard_deviation);
}

void require_matching_sample_containers(const std::vector<double>& observations) {
    const std::deque<double> deque(observations.begin(), observations.end());

    const auto contiguous = sample_moments(std::span<const double>(observations));
    const auto noncontiguous = sample_moments(deque);

    REQUIRE(contiguous.mean == noncontiguous.mean);
    REQUIRE(contiguous.standard_deviation == noncontiguous.standard_deviation);
}

}  // namespace

TEST_CASE("Statistics rejects empty spans and deques") {
    const std::span<const double> empty_span;
    const std::vector<double> empty_vector;
    const std::deque<double> empty_deque;

    REQUIRE_THROWS_AS(population_moments(empty_span), std::invalid_argument);
    REQUIRE_THROWS_AS(
        population_moments(std::span<const double>(empty_vector)),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(population_moments(empty_deque), std::invalid_argument);
}

TEST_CASE("Statistics rejects NaN at any position in a span") {
    const double nan = std::numeric_limits<double>::quiet_NaN();

    for (const std::vector<double>& observations : {
        std::vector<double>{nan, 1.0, 2.0},
        std::vector<double>{1.0, nan, 2.0},
        std::vector<double>{1.0, 2.0, nan}
    }) {
        REQUIRE_THROWS_AS(
            population_moments(std::span<const double>(observations)),
            std::invalid_argument
        );
    }
}

TEST_CASE("Statistics rejects positive and negative infinity in spans") {
    const double infinity = std::numeric_limits<double>::infinity();

    for (const double invalid : {infinity, -infinity}) {
        const std::array<double, 3> observations{1.0, invalid, 3.0};
        REQUIRE_THROWS_AS(
            population_moments(std::span<const double>(observations)),
            std::invalid_argument
        );
    }
}

TEST_CASE("Statistics rejects non-finite observations in deques") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    for (const double invalid : {nan, infinity, -infinity}) {
        const std::deque<double> observations{1.0, invalid, 3.0};
        REQUIRE_THROWS_AS(population_moments(observations), std::invalid_argument);
    }
}

TEST_CASE("Statistics returns the observation and zero deviation for a singleton") {
    for (const double observation : {0.0, 7.5, -9.0, 1e300}) {
        const std::array<double, 1> observations{observation};
        require_moments(
            population_moments(std::span<const double>(observations)),
            observation,
            0.0
        );
        require_moments(population_moments(std::deque<double>{observation}), observation, 0.0);
    }
}

TEST_CASE("Statistics returns zero moments for all-zero observations") {
    const std::vector<double> observations{0.0, -0.0, 0.0, -0.0};
    require_moments(population_moments(std::span<const double>(observations)), 0.0, 0.0);
    require_moments(population_moments(std::deque<double>(observations.begin(), observations.end())), 0.0, 0.0);
}

TEST_CASE("Statistics returns zero deviation for constant finite observations") {
    for (const double constant : {8.0, -12.0, 1e200}) {
        const std::vector<double> observations(21, constant);
        require_moments(
            population_moments(std::span<const double>(observations)),
            constant,
            0.0
        );
        require_matching_containers(observations);
    }
}

TEST_CASE("Statistics matches a simple two-observation population oracle") {
    const std::array<double, 2> observations{2.0, 4.0};
    require_moments(
        population_moments(std::span<const double>(observations)),
        3.0,
        1.0
    );
}

TEST_CASE("Statistics matches the approved Bollinger population moments oracle") {
    const std::array<double, 3> observations{100.0, 102.0, 104.0};
    require_moments(
        population_moments(std::span<const double>(observations)),
        102.0,
        std::sqrt(8.0 / 3.0)
    );
}

TEST_CASE("Statistics uses population rather than sample standard deviation") {
    const std::array<double, 3> observations{1.0, 2.0, 3.0};
    require_moments(
        population_moments(std::span<const double>(observations)),
        2.0,
        std::sqrt(2.0 / 3.0)
    );
}

TEST_CASE("Statistics handles negative positive and zero observations") {
    const std::array<double, 5> observations{-4.0, -2.0, 0.0, 2.0, 4.0};
    require_moments(
        population_moments(std::span<const double>(observations)),
        0.0,
        std::sqrt(8.0)
    );
}

TEST_CASE("Statistics matches an oracle with fractional observations") {
    const std::array<double, 3> observations{1.5, 2.5, 5.0};
    require_moments(
        population_moments(std::span<const double>(observations)),
        3.0,
        std::sqrt(13.0 / 6.0)
    );
}

TEST_CASE("Statistics accepts a non-owning span selecting a subrange") {
    const std::array<double, 7> data{-1000.0, 99.0, 100.0, 101.0, 102.0, 103.0, 1000.0};
    const std::span<const double> subset = std::span<const double>(data).subspan(1, 5);
    require_moments(population_moments(subset), 101.0, std::sqrt(2.0));
}

TEST_CASE("Statistics computes identical results for vector-backed spans and deques") {
    require_matching_containers({100.0, 102.0, 104.0});
    require_matching_containers({-4.0, 2.0, 0.0, -4.0, 2.0, 8.0});
    require_matching_containers({1e16, 1.0, -1e16});
    require_matching_containers({0.0});
}

TEST_CASE("Statistics does not modify either input container") {
    std::vector<double> observations{3.0, 1.0, 4.0, 1.0, 5.0};
    std::deque<double> deque(observations.begin(), observations.end());
    const auto original_vector = observations;
    const auto original_deque = deque;

    (void)population_moments(std::span<const double>(observations));
    (void)population_moments(deque);

    REQUIRE(observations == original_vector);
    REQUIRE(deque == original_deque);
}

TEST_CASE("Statistics repeated queries are deterministic and independent") {
    const std::vector<double> observations{100.0, 104.0, 96.0, 103.0, 97.0};
    const auto input = std::span<const double>(observations);
    const auto expected = population_moments(input);

    for (int iteration = 0; iteration < 20; ++iteration) {
        const auto result = population_moments(input);
        REQUIRE(result.mean == expected.mean);
        REQUIRE(result.standard_deviation == expected.standard_deviation);
    }

    const std::array<double, 2> other{0.0, 2.0};
    require_moments(population_moments(std::span<const double>(other)), 1.0, 1.0);
    const auto again = population_moments(input);
    REQUIRE(again.mean == expected.mean);
    REQUIRE(again.standard_deviation == expected.standard_deviation);
}

TEST_CASE("Statistics standard deviation is invariant under sign reversal") {
    const std::array<double, 3> positive{10.0, 20.0, 30.0};
    const std::array<double, 3> negative{-10.0, -20.0, -30.0};

    require_moments(
        population_moments(std::span<const double>(positive)),
        20.0,
        std::sqrt(200.0 / 3.0)
    );
    require_moments(
        population_moments(std::span<const double>(negative)),
        -20.0,
        std::sqrt(200.0 / 3.0)
    );
}

TEST_CASE("Statistics results are invariant to reversing observation order") {
    std::vector<double> observations{100.0, 105.0, 99.0, 100.0, 101.0, 103.0};
    const auto forward = population_moments(std::span<const double>(observations));
    std::reverse(observations.begin(), observations.end());
    const auto reverse = population_moments(std::span<const double>(observations));

    REQUIRE(reverse.mean == Catch::Approx(forward.mean).epsilon(1e-13));
    REQUIRE(
        reverse.standard_deviation ==
        Catch::Approx(forward.standard_deviation).epsilon(1e-13)
    );
}

TEST_CASE("Statistics retains deviations smaller than a rounded large mean") {
    const double large = 1e16;
    const std::array<double, 2> observations{large, large + 2.0};

    const auto result = population_moments(std::span<const double>(observations));
    REQUIRE(result.mean == Catch::Approx(large).epsilon(1e-15));
    REQUIRE(result.standard_deviation == 1.0);
}

TEST_CASE("Statistics compensated mean retains small terms through cancellation") {
    const std::array<double, 3> observations{1e16, 1.0, -1e16};
    require_moments(
        population_moments(std::span<const double>(observations)),
        1.0 / 3.0,
        std::sqrt(2.0 / 3.0) * 1e16
    );
}

TEST_CASE("Statistics compensated cancellation works with reversed signs") {
    const std::vector<double> observations{-1e16, 1.0, 1e16};
    require_moments(
        population_moments(std::span<const double>(observations)),
        1.0 / 3.0,
        std::sqrt(2.0 / 3.0) * 1e16
    );
    require_matching_containers(observations);
}

TEST_CASE("Statistics handles opposing maximum finite doubles") {
    const double max = std::numeric_limits<double>::max();
    const std::array<double, 2> observations{-max, max};
    require_moments(
        population_moments(std::span<const double>(observations)),
        0.0,
        max,
        1e-14
    );
}

TEST_CASE("Statistics handles a constant series at the maximum finite double") {
    const double max = std::numeric_limits<double>::max();
    const std::deque<double> observations{max, max, max, max};
    require_moments(population_moments(observations), max, 0.0);
}

TEST_CASE("Statistics preserves representable subnormal standard deviations") {
    const double tiny = std::numeric_limits<double>::denorm_min();
    const std::array<double, 2> observations{-tiny, tiny};

    const auto result = population_moments(std::span<const double>(observations));
    REQUIRE(result.mean == 0.0);
    REQUIRE(result.standard_deviation == tiny);
}

TEST_CASE("Statistics handles large finite observations without squaring overflow") {
    const std::array<double, 3> observations{-1e300, 0.0, 1e300};
    require_moments(
        population_moments(std::span<const double>(observations)),
        0.0,
        std::sqrt(2.0 / 3.0) * 1e300
    );
}

TEST_CASE("Statistics agrees with a simple independent oracle on small integer data") {
    std::mt19937 generator{0x51A715u};
    std::uniform_int_distribution<int> lengths{1, 35};
    std::uniform_int_distribution<int> observations_distribution{-50, 50};

    for (int trial = 0; trial < 250; ++trial) {
        const int length = lengths(generator);
        std::vector<double> observations;
        observations.reserve(static_cast<std::size_t>(length));

        double sum = 0.0;
        for (int i = 0; i < length; ++i) {
            const double value = static_cast<double>(observations_distribution(generator));
            observations.push_back(value);
            sum += value;
        }

        const double expected_mean = sum / static_cast<double>(length);
        double sum_squared_deviations = 0.0;
        for (double observation : observations) {
            const double difference = observation - expected_mean;
            sum_squared_deviations += difference * difference;
        }
        const double expected_standard_deviation =
            std::sqrt(sum_squared_deviations / static_cast<double>(length));

        const auto actual = population_moments(std::span<const double>(observations));
        require_moments(actual, expected_mean, expected_standard_deviation, 1e-11);
        require_matching_containers(observations);
    }
}

TEST_CASE("Statistics remains correct on a long balanced sequence") {
    std::vector<double> observations;
    observations.reserve(100000);

    for (int i = 0; i < 50000; ++i) {
        observations.push_back(0.0);
        observations.push_back(2.0);
    }

    require_moments(
        population_moments(std::span<const double>(observations)),
        1.0,
        1.0
    );
    require_matching_containers(observations);
}

// -----------------------------------------------------------------------------
// Sample-moment regression tests (variance denominator N - 1).
// The population tests above remain unchanged in their numerical expectations.
// -----------------------------------------------------------------------------

TEST_CASE("Statistics sample moments reject empty spans and deques") {
    const std::span<const double> empty_span;
    const std::deque<double> empty_deque;

    REQUIRE_THROWS_AS(sample_moments(empty_span), std::invalid_argument);
    REQUIRE_THROWS_AS(sample_moments(empty_deque), std::invalid_argument);
}

TEST_CASE("Statistics sample moments reject singleton spans and deques") {
    for (const double observation : {0.0, 42.0, -1e200}) {
        const std::array<double, 1> values{observation};
        REQUIRE_THROWS_AS(
            sample_moments(std::span<const double>(values)),
            std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            sample_moments(std::deque<double>{observation}),
            std::invalid_argument
        );
    }
}

TEST_CASE("Statistics sample moments reject NaN at every position") {
    const double nan = std::numeric_limits<double>::quiet_NaN();

    for (const std::vector<double>& values : {
        std::vector<double>{nan, 1.0, 2.0},
        std::vector<double>{1.0, nan, 2.0},
        std::vector<double>{1.0, 2.0, nan}
    }) {
        REQUIRE_THROWS_AS(
            sample_moments(std::span<const double>(values)),
            std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            sample_moments(std::deque<double>(values.begin(), values.end())),
            std::invalid_argument
        );
    }
}

TEST_CASE("Statistics sample moments reject positive and negative infinity") {
    const double inf = std::numeric_limits<double>::infinity();

    for (const double invalid : {inf, -inf}) {
        const std::array<double, 3> values{1.0, invalid, 3.0};
        REQUIRE_THROWS_AS(
            sample_moments(std::span<const double>(values)),
            std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            sample_moments(std::deque<double>(values.begin(), values.end())),
            std::invalid_argument
        );
    }
}

TEST_CASE("Statistics sample moments use N minus one for two observations") {
    const std::array<double, 2> values{2.0, 4.0};
    require_moments(
        sample_moments(std::span<const double>(values)),
        3.0,
        std::sqrt(2.0)
    );
    require_moments(
        population_moments(std::span<const double>(values)),
        3.0,
        1.0
    );
}

TEST_CASE("Statistics sample and population moments use different divisors") {
    const std::array<double, 4> values{1.0, 2.0, 3.0, 4.0};
    const auto population = population_moments(std::span<const double>(values));
    const auto sample = sample_moments(std::span<const double>(values));

    require_moments(population, 2.5, std::sqrt(1.25));
    require_moments(sample, 2.5, std::sqrt(5.0 / 3.0));
    REQUIRE(sample.standard_deviation > population.standard_deviation);
}

TEST_CASE("Statistics sample moments match the approved Oracle A") {
    const std::array<double, 5> returns{0.0, 0.05, -0.02, 0.10, 0.0};
    require_moments(
        sample_moments(std::span<const double>(returns)),
        0.026,
        0.04878524367060187
    );
}

TEST_CASE("Statistics sample moments handle signed and fractional data") {
    const std::array<double, 5> signed_values{-4.0, -2.0, 0.0, 2.0, 4.0};
    require_moments(
        sample_moments(std::span<const double>(signed_values)),
        0.0,
        std::sqrt(10.0)
    );

    const std::array<double, 3> fractions{1.5, 2.5, 5.0};
    require_moments(
        sample_moments(std::span<const double>(fractions)),
        3.0,
        std::sqrt(13.0 / 4.0)
    );
}

TEST_CASE("Statistics sample moments return zero for all-zero observations") {
    const std::array<double, 4> values{0.0, -0.0, 0.0, -0.0};
    require_moments(
        sample_moments(std::span<const double>(values)),
        0.0,
        0.0
    );
}

TEST_CASE("Statistics sample moments return zero deviation for constant data") {
    for (const double constant : {8.0, -12.0, 1e200}) {
        const std::vector<double> values(21, constant);
        require_moments(
            sample_moments(std::span<const double>(values)),
            constant,
            0.0
        );
        require_matching_sample_containers(values);
    }
}

TEST_CASE("Statistics sample moments support spans over subranges") {
    const std::array<double, 7> values{
        -1000.0, 99.0, 100.0, 101.0, 102.0, 103.0, 1000.0
    };
    const auto subset = std::span<const double>(values).subspan(1, 5);
    require_moments(sample_moments(subset), 101.0, std::sqrt(2.5));
}

TEST_CASE("Statistics sample moments agree between spans and deques") {
    require_matching_sample_containers({100.0, 102.0, 104.0});
    require_matching_sample_containers({-4.0, 2.0, 0.0, -4.0, 2.0, 8.0});
    require_matching_sample_containers({1e16, 1.0, -1e16});
    require_matching_sample_containers({0.0, 0.0});
}

TEST_CASE("Statistics sample moments preserve their input containers") {
    std::vector<double> values{3.0, 1.0, 4.0, 1.0, 5.0};
    std::deque<double> deque(values.begin(), values.end());
    const auto original_vector = values;
    const auto original_deque = deque;

    (void)sample_moments(std::span<const double>(values));
    (void)sample_moments(deque);

    REQUIRE(values == original_vector);
    REQUIRE(deque == original_deque);
}

TEST_CASE("Statistics sample moments are deterministic across repeated queries") {
    const std::vector<double> values{100.0, 104.0, 96.0, 103.0, 97.0};
    const auto input = std::span<const double>(values);
    const auto reference = sample_moments(input);

    for (int repetition = 0; repetition < 20; ++repetition) {
        const auto actual = sample_moments(input);
        REQUIRE(actual.mean == reference.mean);
        REQUIRE(actual.standard_deviation == reference.standard_deviation);
    }
}

TEST_CASE("Statistics sample dispersion is invariant under sign reversal") {
    const std::array<double, 3> positive{10.0, 20.0, 30.0};
    const std::array<double, 3> negative{-10.0, -20.0, -30.0};

    require_moments(
        sample_moments(std::span<const double>(positive)),
        20.0,
        10.0
    );
    require_moments(
        sample_moments(std::span<const double>(negative)),
        -20.0,
        10.0
    );
}

TEST_CASE("Statistics sample moments are invariant under reversing order") {
    std::vector<double> values{100.0, 105.0, 99.0, 100.0, 101.0, 103.0};
    const auto forward = sample_moments(std::span<const double>(values));
    std::reverse(values.begin(), values.end());
    const auto reverse = sample_moments(std::span<const double>(values));

    REQUIRE(reverse.mean == Catch::Approx(forward.mean).epsilon(1e-13));
    REQUIRE(
        reverse.standard_deviation ==
        Catch::Approx(forward.standard_deviation).epsilon(1e-13)
    );
}

TEST_CASE("Statistics sample moments retain small changes near large means") {
    const double large = 1e16;
    const std::array<double, 2> values{large, large + 2.0};

    const auto actual = sample_moments(std::span<const double>(values));
    REQUIRE(actual.mean == Catch::Approx(large).epsilon(1e-15));
    REQUIRE(actual.standard_deviation == Catch::Approx(std::sqrt(2.0)).epsilon(1e-12));
}

TEST_CASE("Statistics sample compensated mean survives cancellation") {
    const std::array<double, 3> values{1e16, 1.0, -1e16};
    require_moments(
        sample_moments(std::span<const double>(values)),
        1.0 / 3.0,
        1e16
    );
}

TEST_CASE("Statistics sample moments reject overflowing dispersion") {
    const double max = std::numeric_limits<double>::max();
    const std::array<double, 2> values{-max, max};

    REQUIRE_THROWS_AS(
        sample_moments(std::span<const double>(values)),
        std::overflow_error
    );
}

TEST_CASE("Statistics sample moments handle maximum finite constants") {
    const double max = std::numeric_limits<double>::max();
    const std::array<double, 3> values{max, max, max};
    require_moments(
        sample_moments(std::span<const double>(values)),
        max,
        0.0
    );
}

TEST_CASE("Statistics sample moments retain constant subnormal values") {
    const double tiny = std::numeric_limits<double>::denorm_min();
    const std::array<double, 3> values{tiny, tiny, tiny};
    const auto actual = sample_moments(std::span<const double>(values));
    REQUIRE(actual.mean == tiny);
    REQUIRE(actual.standard_deviation == 0.0);
}

TEST_CASE("Statistics sample moments avoid squaring overflow for large inputs") {
    const std::array<double, 3> values{-1e300, 0.0, 1e300};
    require_moments(
        sample_moments(std::span<const double>(values)),
        0.0,
        1e300
    );
}

TEST_CASE("Statistics sample moments match independent integer-data oracles") {
    std::mt19937 generator{0x51A715u};
    std::uniform_int_distribution<int> lengths{2, 35};
    std::uniform_int_distribution<int> values_distribution{-50, 50};

    for (int trial = 0; trial < 250; ++trial) {
        const int length = lengths(generator);
        std::vector<double> values;
        values.reserve(static_cast<std::size_t>(length));

        double sum = 0.0;
        for (int index = 0; index < length; ++index) {
            const double value = static_cast<double>(values_distribution(generator));
            values.push_back(value);
            sum += value;
        }

        const double mean = sum / static_cast<double>(length);
        double sum_of_squares = 0.0;
        for (double value : values) {
            const double deviation = value - mean;
            sum_of_squares += deviation * deviation;
        }

        const double expected_sd =
            std::sqrt(sum_of_squares / static_cast<double>(length - 1));

        require_moments(
            sample_moments(std::span<const double>(values)),
            mean,
            expected_sd,
            1e-11
        );
        require_matching_sample_containers(values);
    }
}

TEST_CASE("Statistics sample moments remain correct over long balanced inputs") {
    std::vector<double> values;
    values.reserve(100'000);

    for (int index = 0; index < 50'000; ++index) {
        values.push_back(0.0);
        values.push_back(2.0);
    }

    require_moments(
        sample_moments(std::span<const double>(values)),
        1.0,
        std::sqrt(100'000.0 / 99'999.0)
    );
    require_matching_sample_containers(values);
}
