#pragma once

#include <cstddef>
#include <optional>

namespace backtester::indicators {

class ExponentialMovingAverage final {
public:
    explicit ExponentialMovingAverage(
        std::size_t period = 20,
        std::optional<double> alpha = std::nullopt
    );

    void update(double observation);

    [[nodiscard]]
    std::optional<double> value() const noexcept;

    [[nodiscard]]
    std::size_t period() const noexcept;

    [[nodiscard]]
    double alpha() const noexcept;

private:
    std::size_t period_;
    double alpha_;

    std::size_t warmup_count_{0};
    double warmup_mean_{0.0};
    double ema_value_{0.0};
};

}  // namespace backtester::indicators