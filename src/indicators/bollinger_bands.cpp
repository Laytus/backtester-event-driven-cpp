#include "backtester/indicators/bollinger_bands.hpp"

#include "backtester/statistics/statistics.hpp"
#include "backtester/indicators/simple_moving_average.hpp"

#include <cmath>
#include <deque>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace backtester::indicators {

BollingerBands::BollingerBands(
    std::size_t period,
    double multiplier
)
    : period_(period),
    multiplier_(multiplier) {
        if (period_ == 0) {
            throw std::invalid_argument(
                "Bollinger Bands period must be strictly positive"
            );
        }

        if (!std::isfinite(multiplier_) || multiplier_ <= 0.0) {
            throw std::invalid_argument(
                "Bollinger Bands multiplier must be finite and strictly positive"
            );
        }
    }

void BollingerBands::update(double observation) {
    if (!std::isfinite(observation)) {
        throw std::invalid_argument(
            "Bollinger Bands observation must be finite"
        );
    }

    auto next_window = window_;
    next_window.push_back(observation);

    if (next_window.size() > period_) {
        next_window.pop_front();
    }

    std::optional<BollingerBandsValue> next_value;

    if (next_window.size() == period_) {
        const auto moments = statistics::population_moments(next_window);

        // FMA computes each band without materializing multiplier * deviation
        // which could overflow even when the final band is representable.
        const double upper = std::fma(
            multiplier_, moments.standard_deviation, moments.mean
        );

        const double lower = std::fma(
            -multiplier_, moments.standard_deviation, moments.mean
        );

        if (!std::isfinite(upper) || !std::isfinite(lower)) {
            throw std::overflow_error(
                "Bollinger Bands value overflow"
            );
        }

        next_value = BollingerBandsValue{
            moments.mean,
            upper,
            lower
        };
    }

    static_assert(std::is_nothrow_swappable_v<std::deque<double>>);
    static_assert(
        std::is_nothrow_swappable_v<std::optional<BollingerBandsValue>>
    );

    using std::swap;
    swap(window_, next_window);
    swap(current_value_, next_value);
}

std::optional<BollingerBandsValue> BollingerBands::value() const noexcept {
    return current_value_;
}

std::size_t BollingerBands::period() const noexcept {
    return period_;
}

double BollingerBands::multiplier() const noexcept {
    return multiplier_;
}

}  // namespace backtester::indicators