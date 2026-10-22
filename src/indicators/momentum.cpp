#include "backtester/indicators/momentum.hpp"

#include <cmath>
#include <stdexcept>

namespace backtester::indicators {

Momentum::Momentum(
    std::size_t period,
    MomentumMode mode
)
    : period_(period),
    mode_(mode) {
        if (period_ == 0) {
            throw std::invalid_argument(
                "Momentum period must be strictly positive"
            );
        }

        if (
            mode_ != MomentumMode::Absolute &&
            mode_ != MomentumMode::Percentage
        ) {
            throw std::invalid_argument(
                "Invalid momentum mode"
            );
        }
    }

void Momentum::update(double observation) {
    if (!std::isfinite(observation)) {
        throw std::invalid_argument(
            "Momentum observation must be finite"
        );
    }

    if (mode_ == MomentumMode::Percentage && observation <= 0.0) {
        throw std::invalid_argument(
            "Percentage momentum requires strictly positive observations"
        );
    }

    if (history_.size() < period_) {
        history_.push_back(observation);
        return;
    }

    const double reference = history_.front();
    const double change = observation - reference;

    if (!std::isfinite(change)) {
        throw std::overflow_error(
            "Momentum price difference overflow"
        );
    }

    const double next_value = mode_ ==
        MomentumMode::Absolute
            ? change
            : (change / reference) * 100.0;

    if (!std::isfinite(next_value)) {
        throw std::overflow_error(
            "Momentum value overflow"
        );
    }

    history_.push_back(observation);
    history_.pop_front();
    current_value_ = next_value;
}

std::optional<double> Momentum::value() const noexcept {
    return current_value_;
}

std::size_t Momentum::period() const noexcept {
    return period_;
}

MomentumMode Momentum::mode() const noexcept {
    return mode_;
}

}  // namespace backtester::indicators