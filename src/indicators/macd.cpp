#include "backtester/indicators/macd.hpp"

#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace backtester::indicators {

MACD::MACD(
    std::size_t fast_period,
    std::size_t slow_period,
    std::size_t signal_period,
    std::optional<double> fast_alpha,
    std::optional<double> slow_alpha,
    std::optional<double> signal_alpha
)
    : fast_ema_(fast_period, fast_alpha),
    slow_ema_(slow_period, slow_alpha),
    signal_ema_(signal_period, signal_alpha) {
        if (fast_period >= slow_period) {
            throw std::invalid_argument(
                "MACD fast period must be strictly less than slow period"
            );
        }
    }

void MACD::update(double observation) {
    if (!std::isfinite(observation)) {
        throw std::invalid_argument(
            "MACD observation must be finite"
        );
    }

    auto next_fast = fast_ema_;
    auto next_slow = slow_ema_;
    auto next_signal = signal_ema_;
    MACDValue next_value{};

    next_fast.update(observation);
    next_slow.update(observation);

    const auto fast_value = next_fast.value();
    const auto slow_value = next_slow.value();

    if (fast_value && slow_value) {
        const double macd_line = *fast_value - *slow_value;

        if (!std::isfinite(macd_line)) {
            throw std::overflow_error(
                "MACD line overflow"
            );
        }

        next_value.macd_line = macd_line;
        next_signal.update(macd_line);

        const auto signal_line = next_signal.value();

        if (signal_line) {
            next_value.signal_line = *signal_line;

            const double histogram = macd_line - *signal_line;

            if (!std::isfinite(histogram)) {
                throw std::overflow_error(
                    "MACD histogram overflow"
                );
            }

            next_value.histogram = histogram;
        }
    }

    // No-throw swaps make the state commit atomic with respect to exceptions.
    static_assert(std::is_nothrow_swappable_v<ExponentialMovingAverage>);
    static_assert(std::is_nothrow_swappable_v<MACDValue>);

    using std::swap;
    swap(fast_ema_, next_fast);
    swap(slow_ema_, next_slow);
    swap(signal_ema_, next_signal);
    swap(current_value_, next_value);
}

MACDValue MACD::value() const noexcept {
    return current_value_;
}

std::size_t MACD::fast_period() const noexcept {
    return fast_ema_.period();
}

std::size_t MACD::slow_period() const noexcept {
    return slow_ema_.period();
}

std::size_t MACD::signal_period() const noexcept {
    return signal_ema_.period();
}

double MACD::fast_alpha() const noexcept {
    return fast_ema_.alpha();
}

double MACD::slow_alpha() const noexcept {
    return slow_ema_.alpha();
}

double MACD::signal_alpha() const noexcept {
    return signal_ema_.alpha();
}

}  // namespace backtester::indicators