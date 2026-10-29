#pragma once

#include <deque>
#include <span>
#include <optional>

namespace backtester::statistics {

struct PopulationMoments final {
    double mean{0.0};
    double standard_deviation{0.0};
};

struct SampleMoments final {
    double mean{0.0};
    double standard_deviation{0.0};
};

struct SampleBivariateMoments final {
    double mean_x{0.0};
    double mean_y{0.0};
    double standard_deviation_x{0.0};
    double standard_deviation_y{0.0};
    double covariance{0.0};
    std::optional<double> correlation;
};

// Population statistics: variance divisor N.
// Requires at least one finite observation.

[[nodiscard]]
PopulationMoments population_moments(std::span<const double> values);

// Supports the non-contiguous deque used by rolling indicators without
// constructing a temp contiguous buffer.
[[nodiscard]]
PopulationMoments population_moments(const std::deque<double>& values);

// Sample statistics: variance divisor N - 1.
// Requires at least two finite observations.

[[nodiscard]]
SampleMoments sample_moments(std::span<const double> values);

[[nodiscard]]
SampleMoments sample_moments(const std::deque<double>& values);

[[nodiscard]]
SampleBivariateMoments sample_bivariate_moments(
    std::span<const double> x,
    std::span<const double> y
);

[[nodiscard]]
SampleBivariateMoments sample_bivariate_moments(
    const std::deque<double>& x,
    const std::deque<double>& y
);

}  // namespace backtester::statistics