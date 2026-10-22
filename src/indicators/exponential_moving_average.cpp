#include "backtester/indicators/exponential_moving_average.hpp"

#include <cmath>
#include <stdexcept>

namespace backtester::indicators {

ExponentialMovingAverage::ExponentialMovingAverage(
    std::size_t period,
    std::optional<double> alpha
)
    : period_(period),
    alpha_(alpha.value_or(
        2.0 / (static_cast<double>(period) + 1.0)
    )) {
        if (period_ == 0) {
            throw std::invalid_argument(
                "EMA period must be strictly positive"
            );
        }

        if (!std::isfinite(alpha_) || alpha_ <= 0.0 || alpha_ > 1.0) {
            throw std::invalid_argument(
                "EMA alpha must be finite and in (0, 1]"
            );
        }
    }

void ExponentialMovingAverage::update(double observation) {
    if (!std::isfinite(observation)) {
        throw std::invalid_argument(
            "EMA observation must be finite"
        );
    }

    if (warmup_count_ < period_) {
        const std::size_t next_count = warmup_count_ + 1;
        const double weight = 1.0 / static_cast<double>(next_count);
        const double next_mean = std::lerp(warmup_mean_, observation, weight);

        if (!std::isfinite(next_mean)) {
            throw std::overflow_error(
                "EMA warm-up mean is not finite"
            );
        }

        warmup_mean_ = next_mean;
        warmup_count_ = next_count;

        if (warmup_count_ == period_) {
            ema_value_ = next_mean;
        }
        return;
    };

    const double next_ema = std::lerp(ema_value_, observation, alpha_);

    if (!std::isfinite(next_ema)) {
        throw std::overflow_error(
            "EMA value is not finite"
        );
    }

    ema_value_ = next_ema;

}

std::optional<double> ExponentialMovingAverage::value() const noexcept {
    if (warmup_count_ < period_) {
        return std::nullopt;
    }

    return ema_value_;
}

std::size_t ExponentialMovingAverage::period() const noexcept {
    return period_;
}

double ExponentialMovingAverage::alpha() const noexcept {
    return alpha_;
}

}  // namespace backtester::indicators