#include "backtester/strategy/momentum_strategy.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace backtester::strategy {

MomentumStrategy::MomentumStrategy(
    std::string symbol,
    std::size_t momentum_period,
    indicators::MomentumMode momentum_mode,
    double momentum_threshold,
    PriceSource price_source
)
    : symbol_(std::move(symbol)),
    momentum_threshold_(momentum_threshold),
    price_source_(price_source),
    momentum_(momentum_period, momentum_mode) {
        if (symbol_.empty()) {
            throw std::invalid_argument(
                "Momentum strategy requires a non-empty symbol"
            );
        }

        if (
            !std::isfinite(momentum_threshold_) ||
            momentum_threshold_ < 0.0
        ) {
            throw std::invalid_argument(
                "Momentum threshold must be finite and non-negative"
            );
        }

        if (
            momentum_mode == indicators::MomentumMode::Percentage &&
            momentum_threshold_ >= 100.0
        ) {
            throw std::invalid_argument(
                "Percentage momentum threshold must be less than 100"
            );
        }

        if (!is_valid_price_source(price_source_)) {
            throw std::invalid_argument(
                "Invalid momentum strategy price source"
            );
        }
    }

std::vector<events::SignalEvent>
MomentumStrategy::on_market(const events::MarketEvent& event) {
    const auto& bar = event.bar();

    if (bar.symbol() != symbol_) {
        return {};
    }

    const double observation = select_price(bar, price_source_);

    momentum_.update(observation);
    const auto momentum_value = momentum_.value();

    if (!momentum_value.has_value()) {
        return {};
    }

    auto desired_target = target_;

    if (*momentum_value > momentum_threshold_) {
        desired_target = events::SignalDirection::Long;
    } else if (*momentum_value < -momentum_threshold_) {
        desired_target = events::SignalDirection::Flat;
    }

    if (desired_target == target_) {
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