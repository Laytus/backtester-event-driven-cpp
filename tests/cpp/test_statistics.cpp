#include "backtester/statistics/statistics.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

using backtester::statistics::population_moments;
using backtester::statistics::sample_moments;
using backtester::statistics::sample_bivariate_moments;

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

// Assert numerical agreement without masking very small nonzero quantities.
void require_bivariate_scalar(
    double actual,
    double expected,
    double relative_tolerance = 1e-11,
    double zero_absolute_tolerance = 1e-14
) {
    REQUIRE(std::isfinite(actual));
    REQUIRE(std::isfinite(expected));

    const double tolerance = expected == 0.0
        ? zero_absolute_tolerance
        : std::abs(expected) * relative_tolerance;

    REQUIRE(std::abs(actual - expected) <= tolerance);
}

template <typename BivariateMoments>
void require_bivariate(
    const BivariateMoments& actual,
    double expected_mean_x,
    double expected_mean_y,
    double expected_sd_x,
    double expected_sd_y,
    double expected_covariance,
    std::optional<double> expected_correlation,
    double relative_tolerance = 1e-11
) {
    REQUIRE(actual.standard_deviation_x >= 0.0);
    REQUIRE(actual.standard_deviation_y >= 0.0);

    require_bivariate_scalar(actual.mean_x, expected_mean_x, relative_tolerance);
    require_bivariate_scalar(actual.mean_y, expected_mean_y, relative_tolerance);
    require_bivariate_scalar(actual.standard_deviation_x, expected_sd_x, relative_tolerance);
    require_bivariate_scalar(actual.standard_deviation_y, expected_sd_y, relative_tolerance);
    require_bivariate_scalar(actual.covariance, expected_covariance, relative_tolerance);

    REQUIRE(actual.correlation.has_value() == expected_correlation.has_value());
    if (expected_correlation) {
        REQUIRE(actual.correlation.has_value());
        require_bivariate_scalar(
            *actual.correlation, *expected_correlation, relative_tolerance
        );
        REQUIRE(*actual.correlation >= -1.0);
        REQUIRE(*actual.correlation <= 1.0);
    }
}

void require_matching_bivariate_containers(
    const std::vector<double>& x,
    const std::vector<double>& y
) {
    const std::deque<double> dx(x.begin(), x.end());
    const std::deque<double> dy(y.begin(), y.end());

    const auto a = sample_bivariate_moments(
        std::span<const double>{x}, std::span<const double>{y}
    );
    const auto b = sample_bivariate_moments(dx, dy);

    REQUIRE(a.mean_x == b.mean_x);
    REQUIRE(a.mean_y == b.mean_y);
    REQUIRE(a.standard_deviation_x == b.standard_deviation_x);
    REQUIRE(a.standard_deviation_y == b.standard_deviation_y);
    REQUIRE(a.covariance == b.covariance);
    REQUIRE(a.correlation == b.correlation);
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

// -----------------------------------------------------------------------------
// Sample bivariate moments: paired means, sample SDs, sample covariance,
// and Pearson correlation. Inputs are aligned by index (not sorted).
// -----------------------------------------------------------------------------

TEST_CASE("Statistics bivariate moments match the approved Oracle D") {
    const std::array<double, 5> strategy{0.00, 0.01, 0.01, 0.02, 0.01};
    const std::array<double, 5> benchmark{-0.02, -0.01, 0.00, 0.01, 0.02};

    const auto actual = sample_bivariate_moments(
        std::span<const double>{strategy},
        std::span<const double>{benchmark}
    );

    require_bivariate(
        actual,
        0.01,
        0.0,
        std::sqrt(0.00005),
        std::sqrt(0.00025),
        0.000075,
        0.670820393249937
    );
}

TEST_CASE("Statistics bivariate moments use sample covariance for two pairs") {
    const std::array<double, 2> x{2.0, 4.0};
    const std::array<double, 2> y{1.0, 3.0};

    const auto actual = sample_bivariate_moments(
        std::span<const double>{x}, std::span<const double>{y}
    );
    require_bivariate(actual, 3.0, 2.0, std::sqrt(2.0), std::sqrt(2.0), 2.0, 1.0);
}

TEST_CASE("Statistics bivariate covariance divides by N minus one") {
    const std::array<double, 3> x{1.0, 2.0, 3.0};
    const std::array<double, 3> y{2.0, 4.0, 6.0};

    const auto actual = sample_bivariate_moments(
        std::span<const double>{x}, std::span<const double>{y}
    );
    require_bivariate(actual, 2.0, 4.0, 1.0, 2.0, 2.0, 1.0);
    // A population covariance would be 4/3, not 2.
    REQUIRE(actual.covariance > 4.0 / 3.0);
}

TEST_CASE("Statistics bivariate correlation is one for positive affine data") {
    const std::array<double, 4> x{1.0, 2.0, 3.0, 4.0};
    const std::array<double, 4> y{7.0, 10.0, 13.0, 16.0};

    require_bivariate(
        sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y}),
        2.5, 11.5, std::sqrt(5.0 / 3.0), 3.0 * std::sqrt(5.0 / 3.0),
        5.0, 1.0
    );
}

TEST_CASE("Statistics bivariate correlation is minus one for negative affine data") {
    const std::array<double, 4> x{1.0, 2.0, 3.0, 4.0};
    const std::array<double, 4> y{8.0, 5.0, 2.0, -1.0};

    require_bivariate(
        sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y}),
        2.5, 3.5, std::sqrt(5.0 / 3.0), 3.0 * std::sqrt(5.0 / 3.0),
        -5.0, -1.0
    );
}

TEST_CASE("Statistics bivariate correlation is zero for orthogonal centered data") {
    const std::array<double, 3> x{-1.0, 0.0, 1.0};
    const std::array<double, 3> y{1.0, 0.0, 1.0};

    require_bivariate(
        sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y}),
        0.0, 2.0 / 3.0, 1.0, std::sqrt(1.0 / 3.0), 0.0, 0.0
    );
}

TEST_CASE("Statistics identical nonconstant series have covariance equal to sample variance") {
    const std::array<double, 3> x{0.0, 0.01, -0.01};
    const auto actual = sample_bivariate_moments(
        std::span<const double>{x}, std::span<const double>{x}
    );

    require_bivariate(actual, 0.0, 0.0, 0.01, 0.01, 0.0001, 1.0);
}

TEST_CASE("Statistics bivariate all-zero series have undefined correlation") {
    const std::array<double, 4> x{0.0, -0.0, 0.0, -0.0};
    const auto actual = sample_bivariate_moments(
        std::span<const double>{x}, std::span<const double>{x}
    );

    require_bivariate(actual, 0.0, 0.0, 0.0, 0.0, 0.0, std::nullopt);
}

TEST_CASE("Statistics bivariate nonzero constants have zero covariance and undefined correlation") {
    const std::array<double, 3> x{5.0, 5.0, 5.0};
    const std::array<double, 3> y{-3.0, -3.0, -3.0};

    require_bivariate(
        sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y}),
        5.0, -3.0, 0.0, 0.0, 0.0, std::nullopt
    );
}

TEST_CASE("Statistics bivariate constant first series permits zero covariance") {
    const std::array<double, 3> x{0.0, 0.0, 0.0};
    const std::array<double, 3> y{-0.01, 0.0, 0.01};

    require_bivariate(
        sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y}),
        0.0, 0.0, 0.0, 0.01, 0.0, std::nullopt
    );
}

TEST_CASE("Statistics bivariate constant second series has undefined correlation") {
    const std::array<double, 3> x{-0.01, 0.0, 0.01};
    const std::array<double, 3> y{2.0, 2.0, 2.0};

    require_bivariate(
        sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y}),
        0.0, 2.0, 0.01, 0.0, 0.0, std::nullopt
    );
}

TEST_CASE("Statistics bivariate covariance is symmetric in the two inputs") {
    const std::array<double, 3> x{-2.0, 0.0, 2.0};
    const std::array<double, 3> y{1.0, 2.0, 3.0};
    const auto xy = sample_bivariate_moments(
        std::span<const double>{x}, std::span<const double>{y}
    );
    const auto yx = sample_bivariate_moments(
        std::span<const double>{y}, std::span<const double>{x}
    );

    require_bivariate(xy, 0.0, 2.0, 2.0, 1.0, 2.0, 1.0);
    require_bivariate(yx, 2.0, 0.0, 1.0, 2.0, 2.0, 1.0);
}

TEST_CASE("Statistics bivariate covariance and correlation reverse sign with one input") {
    const std::array<double, 3> x{-2.0, 0.0, 2.0};
    const std::array<double, 3> y{-1.0, -2.0, -3.0};

    require_bivariate(
        sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y}),
        0.0, -2.0, 2.0, 1.0, -2.0, -1.0
    );
}

TEST_CASE("Statistics bivariate observations remain paired by index") {
    const std::array<double, 3> x{0.0, 1.0, 2.0};
    const std::array<double, 3> y{2.0, 1.0, 0.0};

    require_bivariate(
        sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y}),
        1.0, 1.0, 1.0, 1.0, -1.0, -1.0
    );
}

TEST_CASE("Statistics bivariate moments reject mismatched series lengths") {
    const std::array<double, 2> two{1.0, 2.0};
    const std::array<double, 3> three{1.0, 2.0, 3.0};
    const std::array<double, 0> empty{};

    REQUIRE_THROWS_AS(
        sample_bivariate_moments(std::span<const double>{two}, std::span<const double>{three}),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        sample_bivariate_moments(std::span<const double>{three}, std::span<const double>{two}),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        sample_bivariate_moments(std::span<const double>{empty}, std::span<const double>{two}),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        sample_bivariate_moments(std::deque<double>{1.0, 2.0}, std::deque<double>{1.0}),
        std::invalid_argument
    );
}

TEST_CASE("Statistics bivariate moments reject empty and singleton aligned samples") {
    const std::array<double, 0> empty{};
    const std::array<double, 1> single{42.0};
    const std::deque<double> empty_deque;
    const std::deque<double> single_deque{42.0};

    REQUIRE_THROWS_AS(
        sample_bivariate_moments(std::span<const double>{empty}, std::span<const double>{empty}),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        sample_bivariate_moments(std::span<const double>{single}, std::span<const double>{single}),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(sample_bivariate_moments(empty_deque, empty_deque), std::invalid_argument);
    REQUIRE_THROWS_AS(sample_bivariate_moments(single_deque, single_deque), std::invalid_argument);
}

TEST_CASE("Statistics bivariate moments reject nonfinite values in either series") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const std::array<double, 3> good{1.0, 2.0, 3.0};

    for (const double invalid : {nan, inf, -inf}) {
        for (std::size_t position = 0; position < good.size(); ++position) {
            auto x = good;
            auto y = good;
            x[position] = invalid;
            REQUIRE_THROWS_AS(
                sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{good}),
                std::invalid_argument
            );
            y[position] = invalid;
            REQUIRE_THROWS_AS(
                sample_bivariate_moments(std::span<const double>{good}, std::span<const double>{y}),
                std::invalid_argument
            );
        }
    }
}

TEST_CASE("Statistics bivariate moments reject nonfinite observations in deques") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double invalid : {nan, inf, -inf}) {
        const std::deque<double> x{1.0, invalid, 3.0};
        const std::deque<double> y{2.0, 4.0, 6.0};
        REQUIRE_THROWS_AS(sample_bivariate_moments(x, y), std::invalid_argument);
        REQUIRE_THROWS_AS(sample_bivariate_moments(y, x), std::invalid_argument);
    }
}

TEST_CASE("Statistics bivariate sample SD agrees with the univariate sample API") {
    const std::vector<double> x{-4.0, 0.0, 1.0, 3.0, 8.0};
    const std::vector<double> y{1.0, 10.0, 2.0, 5.0, -6.0};

    const auto bi = sample_bivariate_moments(
        std::span<const double>{x}, std::span<const double>{y}
    );
    const auto mx = sample_moments(std::span<const double>{x});
    const auto my = sample_moments(std::span<const double>{y});

    require_bivariate_scalar(bi.mean_x, mx.mean);
    require_bivariate_scalar(bi.mean_y, my.mean);
    require_bivariate_scalar(bi.standard_deviation_x, mx.standard_deviation);
    require_bivariate_scalar(bi.standard_deviation_y, my.standard_deviation);
}

TEST_CASE("Statistics bivariate moments support matched span subranges") {
    const std::array<double, 7> x{-1000.0, 0.0, 0.01, 0.01, 0.02, 0.01, 1000.0};
    const std::array<double, 7> y{-1000.0, -0.02, -0.01, 0.00, 0.01, 0.02, 1000.0};
    const auto sx = std::span<const double>{x}.subspan(1, 5);
    const auto sy = std::span<const double>{y}.subspan(1, 5);

    require_bivariate(
        sample_bivariate_moments(sx, sy),
        0.01, 0.0, std::sqrt(0.00005), std::sqrt(0.00025),
        0.000075, 0.670820393249937
    );
}

TEST_CASE("Statistics bivariate moments agree between spans and deques") {
    require_matching_bivariate_containers(
        {0.0, 0.01, 0.01, 0.02, 0.01}, {-0.02, -0.01, 0.0, 0.01, 0.02}
    );
    require_matching_bivariate_containers({-4.0, 2.0, 0.0, 8.0}, {1.0, -3.0, 2.0, 9.0});
    require_matching_bivariate_containers({0.0, 0.0, 0.0}, {1.0, 2.0, 3.0});
    require_matching_bivariate_containers({-1e150, 0.0, 1e150}, {-1e140, 0.0, 1e140});
}

TEST_CASE("Statistics bivariate moments do not modify either input") {
    std::vector<double> x{4.0, 1.0, 3.0, 8.0, 2.0};
    std::vector<double> y{2.0, 5.0, 9.0, 7.0, 1.0};
    const std::vector<double> original_x = x;
    const std::vector<double> original_y = y;

    (void)sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y});
    REQUIRE(x == original_x);
    REQUIRE(y == original_y);

    const std::deque<double> dx(x.begin(), x.end());
    const std::deque<double> dy(y.begin(), y.end());
    const auto old_dx = dx;
    const auto old_dy = dy;
    (void)sample_bivariate_moments(dx, dy);
    REQUIRE(dx == old_dx);
    REQUIRE(dy == old_dy);
}

TEST_CASE("Statistics bivariate moments remain deterministic across independent calls") {
    const std::vector<double> x{-4.0, 0.0, 1.0, 3.0, 8.0};
    const std::vector<double> y{1.0, 10.0, 2.0, 5.0, -6.0};
    const auto a = std::span<const double>{x};
    const auto b = std::span<const double>{y};
    const auto original = sample_bivariate_moments(a, b);

    for (int i = 0; i < 20; ++i) {
        const auto again = sample_bivariate_moments(a, b);
        REQUIRE(again.mean_x == original.mean_x);
        REQUIRE(again.mean_y == original.mean_y);
        REQUIRE(again.standard_deviation_x == original.standard_deviation_x);
        REQUIRE(again.standard_deviation_y == original.standard_deviation_y);
        REQUIRE(again.covariance == original.covariance);
        REQUIRE(again.correlation == original.correlation);
    }
}

TEST_CASE("Statistics bivariate moments are invariant to reversing paired observations") {
    std::vector<double> x{1.0, 7.0, -2.0, 3.0, 5.0};
    std::vector<double> y{5.0, -1.0, 4.0, 8.0, 6.0};
    const auto initial = sample_bivariate_moments(
        std::span<const double>{x}, std::span<const double>{y}
    );

    std::reverse(x.begin(), x.end());
    std::reverse(y.begin(), y.end());
    const auto reverse = sample_bivariate_moments(
        std::span<const double>{x}, std::span<const double>{y}
    );

    require_bivariate_scalar(reverse.mean_x, initial.mean_x);
    require_bivariate_scalar(reverse.mean_y, initial.mean_y);
    require_bivariate_scalar(reverse.standard_deviation_x, initial.standard_deviation_x);
    require_bivariate_scalar(reverse.standard_deviation_y, initial.standard_deviation_y);
    require_bivariate_scalar(reverse.covariance, initial.covariance);
    REQUIRE(reverse.correlation.has_value() == initial.correlation.has_value());
    if (initial.correlation) {
        require_bivariate_scalar(*reverse.correlation, *initial.correlation);
    }
}

TEST_CASE("Statistics bivariate moments preserve small differences near large means") {
    const double large = 1e16;
    const std::array<double, 2> x{large, large + 2.0};
    const std::array<double, 2> y{1.0, 2.0};

    require_bivariate(
        sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y}),
        large, 1.5, std::sqrt(2.0), std::sqrt(0.5), 1.0, 1.0
    );
}

TEST_CASE("Statistics bivariate moments handle large finite covariance without overflow") {
    const std::array<double, 3> x{-1e150, 0.0, 1e150};
    const std::array<double, 3> y{-1e140, 0.0, 1e140};

    require_bivariate(
        sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y}),
        0.0, 0.0, 1e150, 1e140, 1e290, 1.0
    );
}

TEST_CASE("Statistics bivariate moments handle scales of opposite magnitudes") {
    const std::array<double, 3> x{-1e300, 0.0, 1e300};
    const std::array<double, 3> y{-1e-300, 0.0, 1e-300};

    const auto actual = sample_bivariate_moments(
        std::span<const double>{x}, std::span<const double>{y}
    );
    require_bivariate(actual, 0.0, 0.0, 1e300, 1e-300, 1.0, 1.0);
    REQUIRE(actual.standard_deviation_y > 0.0);
}

TEST_CASE("Statistics bivariate correlation survives subnormal covariance underflow") {
    const double tiny = std::numeric_limits<double>::denorm_min();
    const std::array<double, 2> x{-tiny, tiny};
    const std::array<double, 2> y{-tiny, tiny};

    const auto actual = sample_bivariate_moments(
        std::span<const double>{x}, std::span<const double>{y}
    );
    REQUIRE(actual.standard_deviation_x > 0.0);
    REQUIRE(actual.standard_deviation_y > 0.0);
    REQUIRE(actual.covariance == 0.0);  // True covariance is below double range.
    REQUIRE(actual.correlation.has_value());
    require_bivariate_scalar(*actual.correlation, 1.0);
}

TEST_CASE("Statistics bivariate moments permit maximum finite constant input") {
    const double maximum = std::numeric_limits<double>::max();
    const std::array<double, 3> x{maximum, maximum, maximum};
    const std::array<double, 3> y{0.0, 1.0, 2.0};

    require_bivariate(
        sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y}),
        maximum, 1.0, 0.0, 1.0, 0.0, std::nullopt
    );
}

TEST_CASE("Statistics bivariate covariance throws when its output overflows") {
    const std::array<double, 3> x{-1e300, 0.0, 1e300};
    const std::array<double, 3> y{-1e300, 0.0, 1e300};
    REQUIRE_THROWS_AS(
        sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y}),
        std::overflow_error
    );
}

TEST_CASE("Statistics bivariate moments reject overflowing sample standard deviation") {
    const double maximum = std::numeric_limits<double>::max();
    const std::array<double, 2> x{-maximum, maximum};
    const std::array<double, 2> y{0.0, 1.0};
    REQUIRE_THROWS_AS(
        sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y}),
        std::overflow_error
    );
}

TEST_CASE("Statistics bivariate moments agree with independent small-integer oracles") {
    std::mt19937 generator{0xB1A712u};
    std::uniform_int_distribution<int> lengths{2, 35};
    std::uniform_int_distribution<int> x_dist{-30, 30};
    std::uniform_int_distribution<int> y_dist{-30, 30};

    for (int trial = 0; trial < 200; ++trial) {
        const int count = lengths(generator);
        std::vector<double> x;
        std::vector<double> y;
        x.reserve(static_cast<std::size_t>(count));
        y.reserve(static_cast<std::size_t>(count));

        double sum_x = 0.0;
        double sum_y = 0.0;
        for (int i = 0; i < count; ++i) {
            const auto xi = static_cast<double>(x_dist(generator));
            const auto yi = static_cast<double>(y_dist(generator));
            x.push_back(xi);
            y.push_back(yi);
            sum_x += xi;
            sum_y += yi;
        }

        const double mean_x = sum_x / static_cast<double>(count);
        const double mean_y = sum_y / static_cast<double>(count);
        double sum_xx = 0.0;
        double sum_yy = 0.0;
        double sum_xy = 0.0;
        for (int i = 0; i < count; ++i) {
            const double dx = x[static_cast<std::size_t>(i)] - mean_x;
            const double dy = y[static_cast<std::size_t>(i)] - mean_y;
            sum_xx += dx * dx;
            sum_yy += dy * dy;
            sum_xy += dx * dy;
        }

        const double divisor = static_cast<double>(count - 1);
        const double sd_x = std::sqrt(sum_xx / divisor);
        const double sd_y = std::sqrt(sum_yy / divisor);
        const double covariance = sum_xy / divisor;
        const std::optional<double> correlation = sd_x > 0.0 && sd_y > 0.0
            ? std::optional<double>{covariance / (sd_x * sd_y)}
            : std::nullopt;

        const auto actual = sample_bivariate_moments(
            std::span<const double>{x}, std::span<const double>{y}
        );
        require_bivariate(
            actual, mean_x, mean_y, sd_x, sd_y, covariance, correlation, 1e-10
        );
    }
}

TEST_CASE("Statistics bivariate moments remain correct on a long balanced series") {
    std::vector<double> x;
    std::vector<double> y;
    x.reserve(100'000);
    y.reserve(100'000);

    for (int i = 0; i < 50'000; ++i) {
        x.push_back(0.0);
        x.push_back(2.0);
        y.push_back(0.0);
        y.push_back(4.0);
    }

    const double correction = 100'000.0 / 99'999.0;
    require_bivariate(
        sample_bivariate_moments(std::span<const double>{x}, std::span<const double>{y}),
        1.0, 2.0, std::sqrt(correction), 2.0 * std::sqrt(correction),
        2.0 * correction, 1.0
    );
}
