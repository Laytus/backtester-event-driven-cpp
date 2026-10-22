#pragma once

#include <cstddef>
#include <deque>
#include <optional>

namespace backtester::indicators {

enum class MomentumMode {
    Absolute,
    Percentage
};

class Momentum final {
public:
    explicit Momentum(
        std::size_t period = 10,
        MomentumMode mode = MomentumMode::Absolute
    );

    void update(double observation);

    [[nodiscard]]
    std::optional<double> value() const noexcept;

    [[nodiscard]]
    std::size_t period() const noexcept;

    [[nodiscard]]
    MomentumMode mode() const noexcept;

private:
    std::size_t period_;
    MomentumMode mode_;

    std::deque<double> history_;
    std::optional<double> current_value_;
};

}  // namespace backtester::indicators