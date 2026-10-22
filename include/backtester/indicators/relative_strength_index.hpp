#pragma once

#include <cstddef>
#include <optional>

namespace backtester::indicators {

enum class RSIMode {
    Directional,
    Magnitude
};

class RelativeStrengthIndex final {
public:
    explicit RelativeStrengthIndex(
        std::size_t period = 14,
        RSIMode mode = RSIMode::Directional,
        std::optional<double> beta = std::nullopt
    );

    void update(double observation);

    [[nodiscard]]
    std::optional<double> value() const noexcept;

    [[nodiscard]]
    std::size_t period() const noexcept;

    [[nodiscard]]
    RSIMode mode() const noexcept;

    [[nodiscard]]
    double beta() const noexcept;

private:
    std::size_t period_;
    RSIMode mode_;
    double beta_;

    std::optional<double> previous_observation_;

    std::size_t warmup_changes_{0};

    double average_up_{0.0};
    double average_down_{0.0};

    std::optional<double> current_value_;
};

}  // namespace backtester::indicators