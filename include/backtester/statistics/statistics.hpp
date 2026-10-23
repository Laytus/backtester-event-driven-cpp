#pragma once

#include <deque>
#include <span>

namespace backtester::statistics {

struct PopulationMoments final {
    double mean;
    double standard_deviation;
};

// Computes the arithmetic mean and population standard deviation.

// Requires a non-empty collection of finite observations.
[[nodiscard]]
PopulationMoments population_moments(std::span<const double> values);

// Supports the non-contiguous deque used by rolling indicators without
// constructing a temp contiguous buffer.
[[nodiscard]]
PopulationMoments population_moments(const std::deque<double>& values);

}  // namespace backtester::statistics