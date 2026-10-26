#include "backtester/strategy/channel_breakout_strategy.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace backtester::strategy {

ChannelBreakoutStrategy::ChannelBreakoutStrategy(
    std::string symbol,
    std::size_t entry_period,
    std::size_t exit_period,
    double breakout_buffer,
    PriceSource reference_price
)
    : symbol_(std::move(symbol)),
    breakout_buffer_(breakout_buffer),
    reference_price_(reference_price),
    upper_channel_(entry_period),
    lower_channel_(exit_period) {
        if (symbol_.empty()) {
            throw std::invalid_argument(
                "Channel breakout requires a non-empty symbol"
            );
        }

        if (
            !std::isfinite(breakout_buffer_) ||
            breakout_buffer_ < 0.0 ||
            breakout_buffer_ >= 1.0
        ) {
            throw std::invalid_argument(
                "Channel breakout buffer must be finite and in [0, 1)"
            );
        }

        if (!is_valid_price_source(reference_price_)) {
            throw std::invalid_argument(
                "Invalid channel breakout reference price"
            );
        }
    }

std::vector<events::SignalEvent>
ChannelBreakoutStrategy::on_market(const events::MarketEvent& event) {
    const auto& bar = event.bar();

    if (bar.symbol() != symbol_) {
        return {};
    }

    const double reference = select_price(bar, reference_price_);

    const auto previous_upper = upper_channel_.value();
    const auto previous_lower = lower_channel_.value();

    auto desired_target = target_;

    if (previous_upper && previous_lower) {
        // UpperThreshold = (1 + B) * Upper
        const double upper_threshold = std::fma(
            breakout_buffer_,
            previous_upper->maximum,
            previous_upper->maximum
        );

        // LowerThreshold = (1 - B) * Lower
        const double lower_threshold = std::fma(
            -breakout_buffer_,
            previous_lower->minimum,
            previous_lower->minimum
        );

        if (reference > upper_threshold) {
            desired_target = events::SignalDirection::Long;
        } else if (reference < lower_threshold) {
            desired_target = events::SignalDirection::Flat;
        }
    }

    upper_channel_.update(bar.high());
    lower_channel_.update(bar.low());

    if (
        !previous_upper.has_value() ||
        !previous_lower.has_value() ||
        desired_target == target_
    ) {
        return {};
    }

    std::vector<events::SignalEvent> signals;
    signals.emplace_back(
        symbol_,
        desired_target,
        bar.session_date()
    );

    target_ = desired_target;

    return signals;
}

}  // namespace backtester::strategy