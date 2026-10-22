#include "backtester/indicators/simple_moving_average.hpp"

#include <stdexcept>
#include <cmath>

namespace backtester::indicators {

namespace {

// Neumaier compensated summation
void compensated_add(
    double value,
    double& sum,
    double& compensation
) {
    const double next_sum = sum + value;

    if (!std::isfinite(next_sum)) {
        throw std::overflow_error(
            "SMA running sum overflow"
        );
    }

    if (std::abs(sum) >= std::abs(value)) {
        compensation += (sum - next_sum) + value;
    } else {
        compensation += (value - next_sum) + sum;
    }

    if (
        !std::isfinite(compensation) ||
        !std::isfinite(next_sum + compensation)
    ) {
        throw std::overflow_error(
            "SMA compensated sum overflow"
        );
    }

    sum = next_sum;
}

}  // namespace

SimpleMovingAverage::SimpleMovingAverage(std::size_t period)
    : period_(period) {
        if (period_ == 0) {
            throw std::invalid_argument(
                "SMA period must be strictly positive"
            );
        }
    }


void SimpleMovingAverage::update(double observation) {
    if (!std::isfinite(observation)) {
        throw std::invalid_argument(
            "SMA observation must be finite"
        );
    }

    double next_sum = running_sum_;
    double next_compensation = compensation_;

    if (window_.size() == period_) {
        compensated_add(
            -window_.front(),
            next_sum,
            next_compensation
        );
    }
    
    compensated_add(
        observation,
        next_sum,
        next_compensation
    );

    window_.push_back(observation);

    if (window_.size() > period_) {
        window_.pop_front();
    }

    running_sum_ = next_sum;
    compensation_ = next_compensation;
}

std::optional<double> SimpleMovingAverage::value() const noexcept {
    if (window_.size() < period_) {
        return std::nullopt;
    }

    const double corrected_sum = running_sum_ + compensation_;

    return corrected_sum / static_cast<double>(period_);
}

std::size_t SimpleMovingAverage::period() const noexcept {
    return period_;
}

}  // namespace backtester::indicators