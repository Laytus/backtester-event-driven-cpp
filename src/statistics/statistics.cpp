#include "backtester/statistics/statistics.hpp"

#include <cmath>
#include <cstddef>
#include <algorithm>
#include <stdexcept>
#include <optional>

namespace backtester::statistics {

namespace {

enum class VarianceConvention {
    Population,
    Sample
};

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

struct MomentsResult final {
    double mean;
    double standard_deviation;
};

template <typename Range>
[[nodiscard]]
MomentsResult moments_impl(
    const Range& values,
    VarianceConvention convention
) {
    const std::size_t count = values.size();

    if (count == 0) {
        throw std::invalid_argument(
            "Statistics input must not be empty"
        );
    }

    const bool sample = convention == VarianceConvention::Sample;

    if (sample && count < 2) {
        throw std::invalid_argument(
            "Sample statistics require at least two observations"
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
            largest_absolute_value,
            std::abs(observation)
        );
    }

    if (largest_absolute_value == 0.0) {
        return {0.0, 0.0};
    }

    int exponent = 0;
    std::frexp(largest_absolute_value, &exponent);

    const double scale = std::ldexp(1.0, exponent - 1);
    const double n = static_cast<double>(count);

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

    const double scaled_mean = raw_sum.value() / n;
    const double mean_offset = offset_sum.value() / n;

    // Population squared deviations in scaled coordinates.
    CompensatedSum squared_deviations;

    for (double observation : values) {
        const double centered = observation / scale - scaled_anchor;
        const double deviation = centered - mean_offset;

        squared_deviations.add(deviation * deviation);
    }

    // Population: N. Sample: N - 1 (Bessel correction).
    const double divisor = sample
        ? static_cast<double>(count - 1)
        : n;

    const double scaled_variance = squared_deviations.value() / divisor;

    const double mean = scaled_mean * scale;
    const double standard_deviation = std::sqrt(scaled_variance) * scale;

    if (!std::isfinite(mean) || !std::isfinite(standard_deviation)) {
        throw std::overflow_error(
            "Statistics result is not finite"
        );
    }

    return {mean, standard_deviation};
}

double power_of_two_scale(double maximum_absolute_value) {
    if (maximum_absolute_value == 0.0) {
        return 1.0;
    }

    int exponent = 0;
    std::frexp(maximum_absolute_value, &exponent);

    return std::ldexp(1.0, exponent - 1);
}

// Multiply 3 finite values while avoiding premature intermediate overflow
// or underflow.
double finite_scaled_product(double a, double b, double c) {
    if (a == 0.0 || b == 0.0 || c == 0.0) {
        return 0.0;
    }

    int ea = 0;
    int eb = 0;
    int ec = 0;

    const double ma = std::frexp(a, &ea);
    const double mb = std::frexp(b, &eb);
    const double mc = std::frexp(c, &ec);

    const double result = std::ldexp(
        (ma * mb) * mc,
        ea + eb + ec
    );

    if (!std::isfinite(result)) {
        throw std::overflow_error(
            "Bivariate statistics produced a nonfinite result"
        );
    }

    return result;
}

template <typename RangeX, typename RangeY>
[[nodiscard]]
SampleBivariateMoments sample_bivariate_moments_impl(
    const RangeX& x,
    const RangeY& y
) {
    if (x.size() != y.size()) {
        throw std::invalid_argument(
            "Bivariate samples must have matching sizes"
        );
    }

    if (x.size() < 2) {
        throw std::invalid_argument(
            "Bivariate sample statistics require at least two observations"
        );
    }

    const std::size_t count = x.size();
    const double n = static_cast<double>(count);

    // Validate observations and determine separate scales.
    double maximum_x = 0.0;
    double maximum_y = 0.0;

    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(x[i]) || !std::isfinite(y[i])) {
            throw std::invalid_argument(
                "Bivariate observations must be finite"
            );
        }

        maximum_x = std::max(maximum_x, std::abs(x[i]));
        maximum_y = std::max(maximum_y, std::abs(y[i]));
    }

    const double scale_x = power_of_two_scale(maximum_x);
    const double scale_y = power_of_two_scale(maximum_y);

    const double anchor_x = x[0] / scale_x;
    const double anchor_y = y[0] / scale_y;

    // Compensated absolute and centered sums.
    CompensatedSum raw_x;
    CompensatedSum raw_y;
    CompensatedSum offsets_x;
    CompensatedSum offsets_y;

    for (std::size_t i = 0; i < count; ++i) {
        const double nx = x[i] / scale_x;
        const double ny = y[i] / scale_y;

        raw_x.add(nx);
        raw_y.add(ny);

        offsets_x.add(nx - anchor_x);
        offsets_y.add(ny - anchor_y);
    }

    const double scaled_mean_x = raw_x.value() / n;
    const double scaled_mean_y = raw_y.value() / n;

    const double center_x = offsets_x.value() / n;
    const double center_y = offsets_y.value() / n;

    // Sample variances and covariance.
    CompensatedSum squared_x;
    CompensatedSum squared_y;
    CompensatedSum cross_products;

    for (std::size_t i = 0; i < count; ++i) {
        const double dx =
            (x[i] / scale_x - anchor_x) - center_x;

        const double dy =
            (y[i] / scale_y - anchor_y) - center_y;

        squared_x.add(dx * dx);
        squared_y.add(dy * dy);
        cross_products.add(dx * dy);
    }

    const double divisor = static_cast<double>(count - 1);

    const double variance_x = squared_x.value() / divisor;
    const double variance_y = squared_y.value() / divisor;
    const double scaled_covariance = cross_products.value() / divisor;

    if (
        !std::isfinite(variance_x) ||
        !std::isfinite(variance_y) ||
        !std::isfinite(scaled_covariance) ||
        variance_x < 0.0 ||
        variance_y < 0.0
    ) {
        throw std::overflow_error(
            "Bivariate statistics produced invalid sample moments"
        );
    }

    const double mean_x = scaled_mean_x * scale_x;
    const double mean_y = scaled_mean_y * scale_y;

    const double sd_x = std::sqrt(variance_x) * scale_x;
    const double sd_y = std::sqrt(variance_y) * scale_y;

    const double covariance = finite_scaled_product(
        scaled_covariance,
        scale_x,
        scale_y
    );

    if (
        !std::isfinite(mean_x) ||
        !std::isfinite(mean_y) ||
        !std::isfinite(sd_x) ||
        !std::isfinite(sd_y)
    ) {
        throw std::overflow_error(
            "Bivariate statistics produced a nonfinite result"
        );
    }

    std::optional<double> correlation;

    if (variance_x > 0.0 && variance_y > 0.0) {
        const double denominator =
            std::sqrt(variance_x) * std::sqrt(variance_y);

        const double value = scaled_covariance / denominator;

        if (!std::isfinite(value)) {
            throw std::overflow_error(
                "Bivariate correlation is not finite"
            );
        }

        // Protect against small floating-point excursions beyond the
        // mathematical interval [-1, 1].
        correlation = std::clamp(value, -1.0, 1.0);
    }

    return {
        mean_x,
        mean_y,
        sd_x,
        sd_y,
        covariance,
        correlation
    };
}

}  // namespace

PopulationMoments population_moments(std::span<const double> values) {
    const auto result = moments_impl(
        values,
        VarianceConvention::Population
    );

    return {result.mean, result.standard_deviation};
}

PopulationMoments population_moments(const std::deque<double>& values) {
    const auto result = moments_impl(
        values,
        VarianceConvention::Population
    );

    return {result.mean, result.standard_deviation};
}

SampleMoments sample_moments(std::span<const double> values) {
    const auto result = moments_impl(
        values,
        VarianceConvention::Sample
    );

    return {result.mean, result.standard_deviation};
}

SampleMoments sample_moments(const std::deque<double>& values) {
    const auto result = moments_impl(
        values,
        VarianceConvention::Sample
    );

    return {result.mean, result.standard_deviation};
}

SampleBivariateMoments sample_bivariate_moments(
    std::span<const double> x,
    std::span<const double> y
) {
    return sample_bivariate_moments_impl(x, y);
}

SampleBivariateMoments sample_bivariate_moments(
    const std::deque<double>& x,
    const std::deque<double>& y
) {
    return sample_bivariate_moments_impl(x, y);
}

}  // namespace backtester::statistics