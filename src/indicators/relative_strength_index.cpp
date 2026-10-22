#include "backtester/indicators/relative_strength_index.hpp"

#include <cmath>
#include <stdexcept>

namespace backtester::indicators {

namespace {

[[nodiscard]]
double resolve_beta(
    std::size_t period,
    RSIMode mode,
    std::optional<double> override_beta
) {
    if (period == 0) {
        throw std::invalid_argument(
            "RSI period must be strictly positive"
        );
    }

    if (
        mode != RSIMode::Directional &&
        mode != RSIMode::Magnitude
    ) {
        throw std::invalid_argument(
            "Invalid RSI mode"
        );
    }

    const double n = static_cast<double>(period);
    const double default_beta = mode == RSIMode::Directional
        ? 2.0 / (n + 1.0)
        : 1.0 / n;
    const double result = override_beta.value_or(default_beta);

    if (
        !std::isfinite(result) ||
        result <= 0.0 ||
        result > 1.0
    ) {
        throw std::invalid_argument(
            "RSI beta must be finite and in (0, 1]"
        );
    }

    return result;
}

[[nodiscard]]
double calculate_rsi(double average_up, double average_down) noexcept {
    if (average_up == 0.0) {
        return average_down == 0.0 ? 50.0 : 0.0;
    }

    if (average_down == 0.0) {
        return 100.0;
    }

    if (average_up >= average_down) {
        return 100.0 / (1.0 + average_down / average_up);
    }

    const double ratio = average_up / average_down;

    return (100.0 * ratio) / (1.0 + ratio);
}

}  // namespace

RelativeStrengthIndex::RelativeStrengthIndex(
    std::size_t period,
    RSIMode mode,
    std::optional<double> beta
)
    : period_(period),
    mode_(mode),
    beta_(resolve_beta(period, mode, beta)) {}

void RelativeStrengthIndex::update(double observation) {
    if (!std::isfinite(observation)) {
        throw std::invalid_argument(
            "RSI observation must be finite"
        );
    }

    // The first price establishes a reference; it is not a price change.
    if (!previous_observation_) {
        previous_observation_ = observation;
        return;
    }

    double up = 0.0;
    double down = 0.0;

    if (mode_ == RSIMode::Directional) {
        up = observation > *previous_observation_ ? 1.0 : 0.0;
        down = observation < *previous_observation_ ? 1.0 : 0.0;
    } else {
        const double change = observation - *previous_observation_;

        if (!std::isfinite(change)) {
            throw std::overflow_error(
                "RSI price difference overflow"
            );
        }

        up = change > 0.0 ? change : 0.0;
        down = change < 0.0 ? -change : 0.0;
    }

    const bool warming_up = warmup_changes_ < period_;
    const std::size_t next_count = warming_up
        ? warmup_changes_ + 1
        : warmup_changes_;

    // During warm-up we compute the arithmetic mean of the first N
    // price changes.
    // Once initialized, use beta to apply exponential smoothing to
    // subsequent observations.
    const double weight = warming_up
        ? 1.0 / static_cast<double>(next_count)
        : beta_;

    const double next_up = std::lerp(average_up_, up, weight);
    const double next_down = std::lerp(average_down_, down, weight);

    if (
        !std::isfinite(next_up) ||
        !std::isfinite(next_down)
    ) {
        throw std::overflow_error(
            "RSI smoothed movement overflow"
        );
    }

    std::optional<double> next_value = current_value_;

    if (next_count == period_) {
        const double rsi = calculate_rsi(next_up, next_down);

        if (!std::isfinite(rsi)) {
            throw std::overflow_error(
                "RSI value is not finite"
            );
        }

        next_value = rsi;
    }

    // Commit only after every potentially failing calculation succeeds.
    previous_observation_ = observation;
    warmup_changes_ = next_count;
    average_up_ = next_up;
    average_down_ = next_down;
    current_value_ = next_value;
}

std::optional<double> RelativeStrengthIndex::value() const noexcept {
    return current_value_;
}

std::size_t RelativeStrengthIndex::period() const noexcept {
    return period_;
}

RSIMode RelativeStrengthIndex::mode() const noexcept {
    return mode_;
}

double RelativeStrengthIndex::beta() const noexcept {
    return beta_;
}

}  // namespace backtester::indicators