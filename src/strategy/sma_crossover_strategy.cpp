#include "backtester/strategy/sma_crossover_strategy.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace backtester::strategy {

SMACrossoverStrategy::SMACrossoverStrategy(
    std::string symbol,
    std::size_t fast_period,
    std::size_t slow_period,
    PriceSource price_source,
    double crossover_threshold
)
    : symbol_(std::move(symbol)),
    price_source_(price_source),
    crossover_threshold_(crossover_threshold),
    fast_sma_(fast_period),
    slow_sma_(slow_period) {
        if (symbol_.empty()) {
            throw std::invalid_argument(
                "SMA crossover requires a non-empty symbol"
            );
        }

        if (fast_period >= slow_period) {
            throw std::invalid_argument(
                "SMA crossover fast period must be smaller than slow period"
            );
        }

        if (!is_valid_price_source(price_source_)) {
            throw std::invalid_argument(
                "Invalid SMA crossover price source"
            );
        }

        if (
            !std::isfinite(crossover_threshold_) || crossover_threshold_ < 0.0
        ) {
            throw std::invalid_argument(
                "SMA crossover threshold must be finite and non-negative"
            );
        }
    }

std::vector<events::SignalEvent>
SMACrossoverStrategy::on_market(const events::MarketEvent& event) {
    const auto& bar = event.bar();

    if (bar.symbol() != symbol_) {
        return {};
    }

    const double observation = select_price(bar, price_source_);
    fast_sma_.update(observation);
    slow_sma_.update(observation);

    const auto fast = fast_sma_.value();
    const auto slow = slow_sma_.value();

    if (!fast || !slow) {
        return {};
    }

    // Both averages should be positive for valid, strictly positive OHLC data.
    if (*slow <= 0.0) {
        throw std::invalid_argument(
            "SMA crossover slow average is not strictly positive"
        );
    }

    const double relative_spread = (*fast - *slow) / *slow;

    if (!std::isfinite(relative_spread)) {
        throw std::overflow_error(
            "SMA crossover relative spread overflow"
        );
    }

    auto desired_target = target_;

    if (relative_spread > crossover_threshold_) {
        desired_target = events::SignalDirection::Long;
    } else if (relative_spread < -crossover_threshold_) {
        desired_target = events::SignalDirection::Flat;
    }

    if (desired_target == target_) {
        return {};
    }

    std::vector<events::SignalEvent> signals;
    signals.emplace_back(symbol_, desired_target, bar.session_date());
    target_ = desired_target;
    return signals;
}

}  // namespace backtester::strategy