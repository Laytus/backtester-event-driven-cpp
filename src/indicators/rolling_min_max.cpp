#include "backtester/indicators/rolling_min_max.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace backtester::indicators {

RollingMinMax::RollingMinMax(std::size_t period)
    : period_(period) {
        if (period_ == 0) {
            throw std::invalid_argument(
                "Rolling Min/Max period must be strictly positive"
            );
        }
    }

void RollingMinMax::update(double observation) {
    if (!std::isfinite(observation)) {
        throw std::invalid_argument(
            "Rolling Min/Max observation must be finite"
        );
    }

    if (
        observations_seen_ == std::numeric_limits<std::size_t>::max()
    ) {
        throw std::overflow_error(
            "Rolling Min/Max observation index overflow"
        );
    }

    const Candidate candidate{observations_seen_, observation};

    // Stage both insertions before removing any existing candidate.
    // If the second insertion fails, undo the first one without changing
    // the previously committed state.
    min_candidates_.push_back(candidate);

    try {
        max_candidates_.push_back(candidate);
    } catch (...) {
        min_candidates_.pop_back();
        throw;
    }

    static_assert(std::is_nothrow_copy_assignable_v<Candidate>);

    while (
        min_candidates_.size() > 1 &&
        min_candidates_[min_candidates_.size() - 2].value >= observation
    ) {
        min_candidates_[min_candidates_.size() - 2] = min_candidates_.back();

        min_candidates_.pop_back();
    }

    while (
        max_candidates_.size() > 1 &&
        max_candidates_[max_candidates_.size() - 2].value <= observation
    ) {
        max_candidates_[max_candidates_.size() - 2] = max_candidates_.back();

        max_candidates_.pop_back();
    }

    const std::size_t oldest_valid = observations_seen_ >= period_ - 1
        ? observations_seen_ - (period_ - 1)
        : 0;

    while (min_candidates_.front().index < oldest_valid) {
        min_candidates_.pop_front();
    }

    while (max_candidates_.front().index < oldest_valid) {
        max_candidates_.pop_front();
    }

    ++observations_seen_;
}

std::optional<RollingMinMaxValue> RollingMinMax::value() const noexcept {
    if (observations_seen_ < period_) {
        return std::nullopt;
    }

    return RollingMinMaxValue{
        min_candidates_.front().value,
        max_candidates_.front().value
    };
}

std::size_t RollingMinMax::period() const noexcept {
    return period_;
}

}  // namespace backtester::indicators