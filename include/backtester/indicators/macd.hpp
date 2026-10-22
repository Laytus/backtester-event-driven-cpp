#pragma once

#include "backtester/indicators/exponential_moving_average.hpp"

#include <cstddef>
#include <optional>

namespace backtester::indicators {

struct MACDValue final {
    std::optional<double> macd_line;
    std::optional<double> signal_line;
    std::optional<double> histogram;
};

class MACD final {
public:
    explicit MACD(
        std::size_t fast_period = 12,
        std::size_t slow_period = 26,
        std::size_t signal_period = 9,
        std::optional<double> fast_alpha = std::nullopt,
        std::optional<double> slowalpha = std::nullopt,
        std::optional<double> signal_alpha = std::nullopt
    );

    void update(double observation);

    [[nodiscard]]
    MACDValue value() const noexcept;

    [[nodiscard]]
    std::size_t fast_period() const noexcept;

    [[nodiscard]]
    std::size_t slow_period() const noexcept;

    [[nodiscard]]
    std::size_t signal_period() const noexcept;

    [[nodiscard]]
    double fast_alpha() const noexcept;

    [[nodiscard]]
    double slow_alpha() const noexcept;

    [[nodiscard]]
    double signal_alpha() const noexcept;

private:
    ExponentialMovingAverage fast_ema_;
    ExponentialMovingAverage slow_ema_;
    ExponentialMovingAverage signal_ema_;

    MACDValue current_value_{};
};

}  // namespace backtester::indicators