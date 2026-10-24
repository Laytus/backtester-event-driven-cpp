#pragma once

#include <cstddef>
#include <deque>
#include <optional>

namespace backtester::indicators {

struct BollingerBandsValue final {
    double middle_band;
    double upper_band;
    double lower_band;
};

class BollingerBands final {
public:
    explicit BollingerBands(
        std::size_t period = 20,
        double multiplier = 2.0
    );

    void update(double observation);

    [[nodiscard]]
    std::optional<BollingerBandsValue> value() const noexcept;

    [[nodiscard]]
    std::size_t period() const noexcept;

    [[nodiscard]]
    double multiplier() const noexcept;

private:
    std::size_t period_;
    double multiplier_;

    std::deque<double> window_;
    std::optional<BollingerBandsValue> current_value_;
};

}  // namespace backtester::indicators