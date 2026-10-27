#pragma once

#include <deque>
#include <span>

namespace backtester::statistics {

struct PopulationMoments final {
    double mean{0.0};
    double standard_deviation{0.0};
};

struct SampleMoments final {
    double mean{0.0};
    double standard_deviation{0.0};
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

}  // namespace backtester::statistics