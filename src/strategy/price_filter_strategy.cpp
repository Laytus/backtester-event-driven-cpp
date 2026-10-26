#include "backtester/strategy/price_filter_strategy.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace backtester::strategy {

PriceFilterStrategy::PriceFilterStrategy(
    std::string symbol,
    std::size_t lookback_period,
    double filter_threshold,
    PriceSource price_source
)
    : symbol_(std::move(symbol)),
    filter_threshold_(filter_threshold),
    price_source_(price_source),
    price_window_(lookback_period) {
        if (symbol_.empty()) {
            throw std::invalid_argument(
                "Price filter requires a non-empty symbol"
            );
        }

        if (
            !std::isfinite(filter_threshold_) ||
            filter_threshold_ < 0.0 ||
            filter_threshold_ >= 1.0
        ) {
            throw std::invalid_argument(
                "Price filter threshold must be finite and in [0, 1)"
            );
        }

        if (!is_valid_price_source(price_source_)) {
            throw std::invalid_argument(
                "Invalid price filter price source"
            );
        }
    }

std::vector<events::SignalEvent>
PriceFilterStrategy::on_market(const events::MarketEvent& event) {
    const auto& bar = event.bar();

    if (bar.symbol() != symbol_) {
        return {};
    }

    const double observation = select_price(bar, price_source_);
    const auto previous_window = price_window_.value();
    auto desired_target = target_;

    if (previous_window.has_value()) {
        const double upper_threshold = std::fma(
            filter_threshold_,
            previous_window->maximum,
            previous_window->maximum
        );

        const double lower_threshold = std::fma(
            -filter_threshold_,
            previous_window->minimum,
            previous_window->minimum
        );

        if (observation > upper_threshold) {
            desired_target = events::SignalDirection::Long;
        } else if (observation < lower_threshold) {
            desired_target = events::SignalDirection::Flat;
        }
    }

    price_window_.update(observation);

    if (
        !previous_window.has_value() ||
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