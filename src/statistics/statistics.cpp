#include "backtester/statistics/statistics.hpp"

#include <cmath>
#include <cstddef>
#include <algorithm>
#include <stdexcept>

namespace backtester::statistics {

namespace {

// Neumaier compensated summation.
struct CompensatedSum final {
    double sum{0.0};
    double correction{0.0};

    void add(double term) noexcept {
        const double next = sum + term;

        if (std::abs(sum) >= std::abs(term)) {
            correction += (sum - next) + term;
        } else {
            correction += (term - next) + sum;
        }

        sum = next;
    }

    [[nodiscard]]
    double value() const noexcept {
        return sum + correction;
    }
};

template <typename Range>
[[nodiscard]]
PopulationMoments population_moments_impl(const Range& values) {
    if (values.empty()) {
        throw std::invalid_argument(
            "Statistics input must not be empty"
        );
    }

    // Select a power-of-two scale.
    double largest_absolute_value = 0.0;

    for (double observation : values) {
        if (!std::isfinite(observation)) {
            throw std::invalid_argument(
                "Statistics observations must be finite"
            );
        }

        largest_absolute_value = std::max(
            largest_absolute_value, std::abs(observation)
        );
    }

    if (largest_absolute_value == 0.0) {
        return {0.0, 0.0};
    }

    int exponent = 0;
    std::frexp(largest_absolute_value, &exponent);
    const double scale = std::ldexp(1.0, exponent - 1);
    const double count = static_cast<double>(values.size());

    // Center at the first observation before accumulating.
    const double anchor = *values.begin();
    const double scaled_anchor = anchor / scale;

    // Compensated sums in both absolute and centered coordinates.
    CompensatedSum raw_sum;
    CompensatedSum offset_sum;

    for (double observation : values) {
        const double normalized = observation / scale;

        raw_sum.add(normalized);
        offset_sum.add(normalized - scaled_anchor);
    }

    const double scaled_mean = raw_sum.value() / count;
    const double mean_offset = offset_sum.value() / count;

    // Population squared deviations in scaled coordinates.
    CompensatedSum squared_deviations;

    for (double observation : values) {
        const double centered = observation / scale - scaled_anchor;
        const double deviation = centered - mean_offset;

        squared_deviations.add(deviation * deviation);
    }

    const double scaled_variance = squared_deviations.value() / count;
    const double mean = scaled_mean * scale;
    const double standard_deviation = std::sqrt(scaled_variance) * scale;

    if (!std::isfinite(mean) || !std::isfinite(standard_deviation)) {
        throw std::overflow_error(
            "Statistics result is not finite"
        );
    }

    return {mean, standard_deviation};
}

}  // namespace

PopulationMoments population_moments(std::span<const double> values) {
    return population_moments_impl(values);
}

PopulationMoments population_moments(const std::deque<double>& values) {
    return population_moments_impl(values);
}

}  // namespace backtester::statistics