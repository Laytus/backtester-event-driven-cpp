#pragma once

#include <cstddef>
#include <deque>
#include <optional>

namespace backtester::indicators {

class SimpleMovingAverage final {
public:
    explicit SimpleMovingAverage(std::size_t period = 20);

    void update(double observation);

    [[nodiscard]]
    std::optional<double> value() const noexcept;

    [[nodiscard]]
    std::size_t period() const noexcept;

private:
    std::size_t period_;
    std::deque<double> window_;

    double running_sum_{0.0};
    double compensation_{0.0};
};

}  // namespace backtester::indicators